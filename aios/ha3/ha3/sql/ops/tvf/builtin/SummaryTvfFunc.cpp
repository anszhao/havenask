#include <ha3/sql/ops/tvf/builtin/SummaryTvfFunc.h>
#include <ha3/sql/data/TableUtil.h>
#include <ha3/sql/ops/tvf/TvfSummaryResource.h>
#include <ha3/sql/ops/util/KernelUtil.h>
#include <ha3/sql/ops/condition/ExprUtil.h>
#include <ha3/sql/util/ValueTypeSwitch.h>
#include <ha3/sql/ops/util/StringConvertor.h>
#include <ha3/queryparser/RequestParser.h>
#include <ha3/common/QueryTermVisitor.h>
#include <suez/turing/expression/util/TypeTransformer.h>
#include <autil/legacy/RapidJsonCommon.h>
#include <ha3/sql/resource/SqlQueryResource.h>

using namespace std;
using namespace autil;
using namespace suez::turing;

USE_HA3_NAMESPACE(common);
USE_HA3_NAMESPACE(queryparser);
USE_HA3_NAMESPACE(summary);
USE_HA3_NAMESPACE(search);
USE_HA3_NAMESPACE(config);

BEGIN_HA3_NAMESPACE(sql);
const string summaryTvfDef = R"tvf_json(
{
  "function_version": 1,
  "function_name": "summaryTvf",
  "function_type": "TVF",
  "is_deterministic": 1,
  "function_content_version": "json_default_0.1",
  "function_content": {
    "properties": {
      "enable_shuffle": false
    },
    "prototypes": [
      {
        "params": {
          "scalars": [
             {
               "type":"string"
             },
             {
               "type":"string"
             }
         ],
          "tables": [
            {
              "auto_infer": true
            }
          ]
        },
        "returns": {
          "new_fields": [
          ],
          "tables": [
            {
              "auto_infer": true
            }
          ]
        }
      }
    ]
  }
}
)tvf_json";

SummaryTvfFunc::SummaryTvfFunc()
    : _summaryExtractorChain(nullptr)
    , _summaryExtractorProvider(nullptr)
    , _summaryProfile(nullptr)
    , _summaryQueryInfo(nullptr)
    , _resource(nullptr)
    , _queryPool(nullptr)
{
}

SummaryTvfFunc::~SummaryTvfFunc() {
    if (_summaryExtractorChain != nullptr) {
        _summaryExtractorChain->endRequest();
        delete _summaryExtractorChain;
    }
    DELETE_AND_SET_NULL(_summaryExtractorProvider);
    DELETE_AND_SET_NULL(_summaryQueryInfo);
    DELETE_AND_SET_NULL(_resource);
}

bool SummaryTvfFunc::init(TvfFuncInitContext &context) {
    _queryPool = context.queryPool;
    _outputFields = context.outputFields;
    _outputFieldsType = context.outputFieldsType;
    auto tvfSummaryResource =
        context.tvfResourceContainer->get<TvfSummaryResource>("TvfSummaryResource");
    if (tvfSummaryResource == nullptr) {
        SQL_LOG(ERROR, "no tvf summary resource");
        return false;
    }
    auto summaryProfileMgrPtr = tvfSummaryResource->getSummaryProfileManager();
    if (summaryProfileMgrPtr == nullptr) {
        SQL_LOG(ERROR, "get summary profile manager failed");
        return false;
    }
    _summaryProfile = summaryProfileMgrPtr->getSummaryProfile(getName());
    if (_summaryProfile == nullptr) {
        SQL_LOG(ERROR, "not find summary_profile_name [%s]", getName().c_str());
        return false;
    }
    SQL_LOG(DEBUG, "use summary_profile_name [%s]", getName().c_str());
    if (!prepareResource(context)) {
        return false;
    }
    return true;
}

bool SummaryTvfFunc::prepareResource(TvfFuncInitContext &context) {
    if (context.params.size() != 2) {
        SQL_LOG(ERROR, "summary tvf need 2 params");
        return false;
    }
    _request.reset(new Request());
    auto *configClause = new ConfigClause();
    _request->setConfigClause(configClause);
    auto *kvpairClause = new KVPairClause();
    kvpairClause->setOriginalString(context.params[0]);
    _request->setKVPairClause(kvpairClause);
    auto *queryClause = new QueryClause();
    queryClause->setOriginalString(context.params[1]);
    _request->setQueryClause(queryClause);

    RequestParser requestParser;
    if (!requestParser.parseKVPairClause(_request)) {
        SQL_LOG(ERROR, "parse kvpair failed, error msg [%s]",
                requestParser.getErrorResult().getErrorMsg().c_str());
        return false;
    }
    if (!context.params[1].empty() &&
        !requestParser.parseQueryClause(_request, *context.queryInfo))
    {
        SQL_LOG(ERROR, "parse query failed, error msg [%s]",
                requestParser.getErrorResult().getErrorMsg().c_str());
        return false;
    }

    DocIdClause *docIdClause = new DocIdClause();
    docIdClause->setQueryString(context.params[1]);
    QueryTermVisitor termVisitor;
    if (queryClause->getQueryCount() > 1) {
        SQL_LOG(ERROR, "not support layer query");
    }
    Query *query = queryClause->getRootQuery();
    if (query) {
        query->accept(&termVisitor);
    }
    docIdClause->setTermVector(termVisitor.getTermVector());
    _request->setDocIdClause(docIdClause);

    _summaryQueryInfo = new SummaryQueryInfo(
            context.params[1], docIdClause->getTermVector());

    common::Ha3MatchDocAllocatorPtr matchDocAllocator(
            new common::Ha3MatchDocAllocator(_queryPool, false));
    _resource = new SearchCommonResource(
            nullptr,
            {},
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            {},
            {},
            context.queryResource->getSuezCavaAllocator(),
            {},
            matchDocAllocator);
    return true;
}

void SummaryTvfFunc::toSummarySchema(const TablePtr &input) {
    TableInfo tableInfo;
    FieldInfos *fieldInfos = new FieldInfos();
    SummaryInfo *summaryInfo = new SummaryInfo();
    for (size_t i = 0; i < input->getColumnCount(); i++) {
        FieldInfo *fieldInfo = new FieldInfo();
        const string &fieldName = input->getColumnName(i);
        fieldInfo->fieldName = fieldName;
        matchdoc::ValueType vt = input->getColumnType(i);
        fieldInfo->isMultiValue = vt.isMultiValue();
        fieldInfo->fieldType = TypeTransformer::transform(vt.getBuiltinType());
        fieldInfos->addFieldInfo(fieldInfo);
        summaryInfo->addFieldName(fieldName);
    }
    tableInfo.setFieldInfos(fieldInfos);
    tableInfo.setSummaryInfo(summaryInfo);
    _hitSummarySchema.reset(new HitSummarySchema(&tableInfo));
}

void SummaryTvfFunc::toSummaryHits(const TablePtr &input,
                                   vector<SummaryHit *> &summaryHits)
{
    for (size_t i = 0; i < input->getRowCount(); ++i) {
        SummaryHit *summaryHit = new SummaryHit(_hitSummarySchema.get(), _queryPool);
        auto summaryDoc = summaryHit->getSummaryDocument();
        for (size_t j = 0; j < input->getColumnCount(); ++j) {
            ColumnPtr column = input->getColumn(j);
            string fieldValue = column->toOriginString(i);
            summaryDoc->SetFieldValue(j, fieldValue.c_str(), fieldValue.size());
        }
        summaryHits.push_back(summaryHit);
    }
}

template <typename T>
bool SummaryTvfFunc::fillSummaryDocs(ColumnPtr &column,
                                     vector<SummaryHit *> &summaryHits,
                                     const TablePtr &input,
                                     summaryfieldid_t summaryFieldId)
{
    ColumnData<T> *data = column->getColumnData<T>();
    for (size_t i = 0; i < summaryHits.size(); ++i) {
        const ConstString *value =
            summaryHits[i]->getSummaryDocument()->GetFieldValue(summaryFieldId);
        T val;
        if (value == nullptr || value->size() == 0) {
            // default construct
            InitializeIfNeeded<T>()(val);
        } else {
            if (!StringConvertor::fromString(value, val, _queryPool)) {
                SQL_LOG(ERROR, "summary value [%s], size [%ld], type [%s] convert failed",
                        value->toString().c_str(), value->size(), typeid(val).name());
                InitializeIfNeeded<T>()(val);
            }
        }
        data->set(i, val);
    }
    return true;
}


bool SummaryTvfFunc::fromSummaryHits(const TablePtr &input,
                                     vector<SummaryHit *> &summaryHits)
{
    input->clearRows();
    for (size_t i = 0; i < _outputFields.size(); ++i) {
        if (!input->declareColumn(_outputFields[i],
                        ExprUtil::transSqlTypeToVariableType(_outputFieldsType[i]),
                        false, false))
        {
            SQL_LOG(ERROR, "declare column for field [%s], type [%s] failed",
                    _outputFields[i].c_str(), _outputFieldsType[i].c_str());
            return false;
        }
    }
    input->batchAllocateRow(summaryHits.size());
    for (size_t i = 0; i < input->getColumnCount(); ++i) {
        ColumnPtr column = input->getColumn(i);
        ColumnSchema *columnSchema = column->getColumnSchema();
        string columnName = columnSchema->getName();
        summaryfieldid_t summaryFieldId = _hitSummarySchema->getSummaryFieldId(columnName);
        if (summaryFieldId == INVALID_SUMMARYFIELDID) {
            SQL_LOG(ERROR, "get summary field [%s] failed", columnName.c_str());
            return false;
        }
        auto vt = columnSchema->getType();
        auto func = [&](auto a) {
            typedef typename decltype(a)::value_type T;
            return this->fillSummaryDocs<T>(column, summaryHits, input, summaryFieldId);
        };
        if (!ValueTypeSwitch::switchType(vt, func, func)) {
            SQL_LOG(ERROR, "fill summary docs column[%s] failed", columnName.c_str());
            return false;
        }
    }
    return true;
}

bool SummaryTvfFunc::compute(const TablePtr &input, bool eof, TablePtr &output) {
    if (_hitSummarySchema == nullptr) {
        toSummarySchema(input);
        _summaryExtractorProvider = new SummaryExtractorProvider(
                _summaryQueryInfo,
                &_summaryProfile->getFieldSummaryConfig(),
                _request.get(),
                nullptr, // attr expr creator not need
                _hitSummarySchema.get(), // will modify schema
                *_resource);

        _summaryExtractorChain = _summaryProfile->createSummaryExtractorChain();
        if (!_summaryExtractorChain->beginRequest(_summaryExtractorProvider)) {
            return false;
        }
    }
    vector<SummaryHit *> summaryHits;
    toSummaryHits(input, summaryHits);
    for (auto summaryHit : summaryHits) {
        _summaryExtractorChain->extractSummary(*summaryHit);
    }
    if (!fromSummaryHits(input, summaryHits)) {
        return false;
    }
    output = input;
    for (auto summaryHit : summaryHits) {
        DELETE_AND_SET_NULL(summaryHit);
    }
    return true;
}

SummaryTvfFuncCreator::SummaryTvfFuncCreator()
    : TvfFuncCreator(summaryTvfDef)
{}

SummaryTvfFuncCreator::~SummaryTvfFuncCreator() {
}

TvfFunc *SummaryTvfFuncCreator::createFunction(const SqlTvfProfileInfo &info) {
    return new SummaryTvfFunc();
}

END_HA3_NAMESPACE(sql);
