#include "indexlib/partition/remote_access/index_field_convertor.h"
#include "indexlib/document/index_document/normal_document/index_document.h"
#include "indexlib/document/index_document/normal_document/index_tokenize_field.h"
#include "indexlib/document/index_document/normal_document/index_raw_field.h"
#include "indexlib/common/field_format/spatial/spatial_field_encoder.h"
#include "indexlib/common/field_format/date/date_field_encoder.h"
#include "indexlib/common/field_format/range/range_field_encoder.h"
#include "indexlib/util/key_hasher_factory.h"
#include "indexlib/config/index_partition_schema.h"

using namespace std;
using namespace autil;
using namespace autil::mem_pool;

IE_NAMESPACE_USE(document);
IE_NAMESPACE_USE(config);
IE_NAMESPACE_USE(util);
IE_NAMESPACE_USE(common);

IE_NAMESPACE_BEGIN(partition);
IE_LOG_SETUP(partition, IndexFieldConvertor);

IndexFieldConvertor::IndexFieldConvertor(
        const IndexPartitionSchemaPtr &schema, regionid_t regionId)
    : mSchema(schema)
    , mRegionId(regionId)
{
    init();
}

IndexFieldConvertor::~IndexFieldConvertor() { 
    HasherVector::iterator iter = mFieldTokenHasherVec.begin();
    for (; iter != mFieldTokenHasherVec.end(); iter++) {
        if (*iter) {
            delete *iter;
        }
    }
    mFieldTokenHasherVec.clear();
}

void IndexFieldConvertor::init() {
    initFieldTokenHasherVector();
    mSpatialFieldEncoder.reset(new SpatialFieldEncoder(mSchema));
    mDateFieldEncoder.reset(new DateFieldEncoder(mSchema));
    mRangeFieldEncoder.reset(new RangeFieldEncoder(mSchema));
}

void IndexFieldConvertor::convertIndexField(const IndexDocumentPtr &indexDoc,
        const FieldConfigPtr &fieldConfig, const string& fieldValue,
        const string& fieldSep, Pool* pool)
{
    FieldType fieldType = fieldConfig->GetFieldType();
    if (fieldType == ft_raw)
    {
        auto rawField = dynamic_cast<IndexRawField*>(indexDoc->CreateField(
                        fieldConfig->GetFieldId(), Field::FieldTag::RAW_FIELD));
        assert(rawField);
        rawField->SetData(ConstString(fieldValue, pool));
        return;
    }
    
    Field* field = indexDoc->CreateField(fieldConfig->GetFieldId(), Field::FieldTag::TOKEN_FIELD);
    auto tokenizeField = dynamic_cast<IndexTokenizeField*>(field);
    pos_t lastTokenPos = 0;
    pos_t curPos = -1;

    fieldid_t fieldId = fieldConfig->GetFieldId();
    if (mSpatialFieldEncoder->IsSpatialIndexField(fieldId))
    {
        addSpatialSection(fieldId, fieldValue, tokenizeField);
        return;
    }

    if (mDateFieldEncoder->IsDateIndexField(fieldId))
    {
        addDateSection(fieldId, fieldValue, tokenizeField);
        return;
    }

    if (mRangeFieldEncoder->IsRangeIndexField(fieldId))
    {
        addRangeSection(fieldId, fieldValue, tokenizeField);
        return;
    }
    addSection(tokenizeField, fieldValue, fieldSep,
               pool, fieldConfig->GetFieldId(), lastTokenPos, curPos);
}

bool IndexFieldConvertor::addSection(IndexTokenizeField *field,
                                     const string& fieldValue, const string& fieldSep, 
                                     Pool *pool, fieldid_t fieldId,
                                     pos_t &lastTokenPos, pos_t &curPos)
{
    TokenizeSectionPtr tokenizeSection = ParseSection(fieldValue, fieldSep);
    Section *indexSection = field->CreateSection(tokenizeSection->GetTokenCount());
    if (indexSection == NULL) {
        IE_LOG(DEBUG, "Failed to create new section.");
        return false;
    }
    TokenizeSection::Iterator it = tokenizeSection->Begin();
    for (; it != tokenizeSection->End(); ++it)
    {
        curPos++;
        if (!addToken(indexSection, *it, pool, fieldId, lastTokenPos, curPos)) {
            break;
        }
    }
    indexSection->SetLength(tokenizeSection->GetTokenCount());
    curPos++;
    return true;
}

void IndexFieldConvertor::addSection(const vector<dictkey_t>& dictKeys,
        IndexTokenizeField* field)
{
    Section *indexSection = field->CreateSection(dictKeys.size());
    if (indexSection == NULL) {
        IE_LOG(DEBUG, "Failed to create new section.");
        return;
    }
    for (size_t i = 0; i < dictKeys.size(); i++)
    {
        pos_t lastPos = 0;
        pos_t curPos = 0;
        addHashToken(indexSection, dictKeys[i], lastPos, curPos);
    }
}

void IndexFieldConvertor::addDateSection(fieldid_t fieldId,
        const string& fieldValue, IndexTokenizeField* field)
{
    vector<dictkey_t> dictKeys;
    mDateFieldEncoder->Encode(fieldId, fieldValue, dictKeys);
    addSection(dictKeys, field);
}

void IndexFieldConvertor::addRangeSection(fieldid_t fieldId,
        const string& fieldValue, IndexTokenizeField* field)
{
    vector<dictkey_t> dictKeys;
    mRangeFieldEncoder->Encode(fieldId, fieldValue, dictKeys);
    addSection(dictKeys, field);
}

void IndexFieldConvertor::addSpatialSection(fieldid_t fieldId,
        const string& fieldValue, IndexTokenizeField* field)
{
    vector<dictkey_t> dictKeys;
    mSpatialFieldEncoder->Encode(fieldId, fieldValue, dictKeys);
    addSection(dictKeys, field);
}

bool IndexFieldConvertor::addToken(
        Section *indexSection, const std::string& token,
        Pool *pool, fieldid_t fieldId, pos_t &lastTokenPos, pos_t &curPos)
{
    const KeyHasher *hasher = mFieldTokenHasherVec[fieldId];
    uint64_t hashKey;
    if (!hasher->GetHashKey(token.c_str(), hashKey)) {
        return true;
    }
    return addHashToken(indexSection, hashKey, lastTokenPos, curPos);;
}

bool IndexFieldConvertor::addHashToken(
        Section *indexSection, dictkey_t hashKey, pos_t &lastTokenPos, pos_t &curPos)
{
    Token* indexToken = indexSection->CreateToken(hashKey);
    if (!indexToken) {
        curPos--;
        IE_LOG(INFO, "token count overflow in one section");
        return false;
    }
    indexToken->SetPosIncrement(curPos - lastTokenPos);
    lastTokenPos = curPos;
    return true;    
}

void IndexFieldConvertor::initFieldTokenHasherVector() {
    const FieldSchemaPtr &fieldSchemaPtr = mSchema->GetFieldSchema(mRegionId);
    const IndexSchemaPtr &indexSchemaPtr = mSchema->GetIndexSchema(mRegionId);
    if (!indexSchemaPtr) {
        return;
    }
    mFieldTokenHasherVec.resize(fieldSchemaPtr->GetFieldCount());
    for (FieldSchema::Iterator it = fieldSchemaPtr->Begin();
         it != fieldSchemaPtr->End(); ++it)
    {
        const FieldConfigPtr &fieldConfig = *it;
        fieldid_t fieldId = fieldConfig->GetFieldId();
        if (!indexSchemaPtr->IsInIndex(fieldId)) {
            continue;
        }
        FieldType fieldType = fieldConfig->GetFieldType();
        KeyHasher *hasher = KeyHasherFactory::CreateByFieldType(fieldType);
        assert(hasher);
        mFieldTokenHasherVec[fieldId] = hasher;
    }
}

IndexFieldConvertor::TokenizeSectionPtr IndexFieldConvertor::ParseSection(
        const string& sectionStr, const string& sep)
{
    TokenizeSection::TokenVector tokenVec = StringUtil::split(sectionStr, sep);
    TokenizeSectionPtr tokenizeSection(new TokenizeSection);
    tokenizeSection->SetTokenVector(tokenVec);
    return tokenizeSection;
}

IE_NAMESPACE_END(partition);

