#include "indexlib/testlib/ExtendDocField2IndexDocField.h"
#include "indexlib/testlib/document_parser.h"
#include "indexlib/common/field_format/attribute/attribute_convertor_factory.h"
#include "indexlib/document/index_document/normal_document/normal_document.h"

using namespace std;
using namespace autil;
using namespace autil::mem_pool;

IE_NAMESPACE_USE(util);
IE_NAMESPACE_USE(common);
IE_NAMESPACE_USE(config);
IE_NAMESPACE_USE(document);

IE_NAMESPACE_BEGIN(testlib);
IE_LOG_SETUP(testlib, ExtendDocField2IndexDocField);

ExtendDocField2IndexDocField::ExtendDocField2IndexDocField(
        const IndexPartitionSchemaPtr &schema, regionid_t regionId)
    : mSchema(schema)
    , mRegionId(regionId)
    , mIndexFieldConvertor(schema, regionId)
{
    init();
}

ExtendDocField2IndexDocField::~ExtendDocField2IndexDocField() {}

void ExtendDocField2IndexDocField::init() {
    initAttrConvert();
}

void ExtendDocField2IndexDocField::convertAttributeField(
    const NormalDocumentPtr &document,
    const FieldConfigPtr &fieldConfig, const RawDocumentPtr& rawDoc)
{
    NormalDocumentPtr doc = DYNAMIC_POINTER_CAST(NormalDocument, document);
    
    fieldid_t fieldId = fieldConfig->GetFieldId();
    if ((size_t)fieldId >= mAttrConvertVec.size()) {
        IE_LOG(ERROR, "field config error: fieldName[%s], fieldId[%d]",
               fieldConfig->GetFieldName().c_str(), fieldId);
        return;
    }
    const AttributeConvertorPtr &convertor = mAttrConvertVec[fieldId];
    assert(convertor);
    const string &fieldValue = rawDoc->GetField(fieldConfig->GetFieldName());
    if (fieldValue.empty() && doc->GetDocOperateType() == UPDATE_FIELD)
    {
        return;
    }
    
    string parsedFieldValue = fieldValue;
    if (fieldConfig->IsMultiValue())
    {
        bool replaceSep = (fieldConfig->GetFieldType() != ft_location &&
                           fieldConfig->GetFieldType() != ft_line &&
                           fieldConfig->GetFieldType() != ft_polygon);

        parsedFieldValue = 
            DocumentParser::ParseMultiValueField(fieldValue, replaceSep);
    }
    
    AttributeDocumentPtr attributeDoc = doc->GetAttributeDocument();
    ConstString convertedValue = convertor->Encode(ConstString(parsedFieldValue),
            doc->GetPool());
    attributeDoc->SetField(fieldId, convertedValue);
}

void ExtendDocField2IndexDocField::convertIndexField(
    const NormalDocumentPtr &document,
    const FieldConfigPtr &fieldConfig, const RawDocumentPtr& rawDoc)
{
    NormalDocumentPtr doc = DYNAMIC_POINTER_CAST(NormalDocument, document);
    string fieldValue = rawDoc->GetField(fieldConfig->GetFieldName());

    IndexDocumentPtr indexDoc = doc->GetIndexDocument();
    // TODO: use fieldConfig->GetFieldTag()
    convertIndexField(indexDoc, fieldConfig, fieldValue,
                      DocumentParser::DP_TOKEN_SEPARATOR, doc->GetPool());
}

void ExtendDocField2IndexDocField::convertIndexField(const IndexDocumentPtr &indexDoc,
        const FieldConfigPtr &fieldConfig, const string& fieldValue,
        const string& fieldSep, Pool* pool)
{
    mIndexFieldConvertor.convertIndexField(indexDoc, fieldConfig, fieldValue, fieldSep, pool);
}

void ExtendDocField2IndexDocField::convertModifyFields(
    const NormalDocumentPtr &document, const string& modifyFields)
{
    NormalDocumentPtr doc = DYNAMIC_POINTER_CAST(NormalDocument, document);
    vector<string> fieldNames;
    StringUtil::fromString(modifyFields, fieldNames, MODIFY_FIELDS_SEP);
    const IndexPartitionSchemaPtr& subSchema =
        mSchema->GetSubIndexPartitionSchema();
    FieldSchemaPtr subFieldSchema;
    if (subSchema)
    {
        subFieldSchema = subSchema->GetFieldSchema();
    }
    FieldSchemaPtr fieldSchema = mSchema->GetFieldSchema();
    for (size_t i = 0; i < fieldNames.size(); i++)
    {
        const string& fieldName = fieldNames[i];
        if (fieldSchema->IsFieldNameInSchema(fieldName))
        {
            fieldid_t fid = fieldSchema->GetFieldId(fieldName);
            assert(fid != INVALID_FIELDID);
            doc->AddModifiedField(fid);
        }
        else if (subFieldSchema)
        {
            fieldid_t fid = subFieldSchema->GetFieldId(fieldName);
            assert(fid != INVALID_FIELDID);
            doc->AddSubModifiedField(fid);
        }
    }
}

void ExtendDocField2IndexDocField::convertSummaryField(
        const SummaryDocumentPtr& doc, 
        const FieldConfigPtr &fieldConfig, 
        const RawDocumentPtr& rawDoc)
{
    fieldid_t fieldId = fieldConfig->GetFieldId();
    const string &fieldValue = rawDoc->GetField(fieldConfig->GetFieldName());
    string parsedFieldValue = fieldValue;
    if (fieldConfig->IsMultiValue())
    {
        bool replaceSep = (fieldConfig->GetFieldType() != ft_location &&
                           fieldConfig->GetFieldType() != ft_line &&
                           fieldConfig->GetFieldType() != ft_line);
        parsedFieldValue = DocumentParser::ParseMultiValueField(fieldValue, replaceSep);
    }

    rawDoc->SetField(fieldConfig->GetFieldName(), parsedFieldValue);
    const string& newFieldValue = rawDoc->GetField(fieldConfig->GetFieldName());
    doc->SetField(fieldId, ConstString(newFieldValue));
}

void ExtendDocField2IndexDocField::initAttrConvert() {
    const FieldSchemaPtr &fieldSchemaPtr = mSchema->GetFieldSchema(mRegionId);
    if (!fieldSchemaPtr) {
        return;
    }
    mAttrConvertVec.resize(fieldSchemaPtr->GetFieldCount());
    const AttributeSchemaPtr &attrSchemaPtr = mSchema->GetAttributeSchema(mRegionId);
    if (!attrSchemaPtr) {
        return;
    }
    for (AttributeSchema::Iterator it = attrSchemaPtr->Begin();
         it != attrSchemaPtr->End(); ++it)
    {
        const FieldConfigPtr &fieldConfigPtr = (*it)->GetFieldConfig();
        mAttrConvertVec[fieldConfigPtr->GetFieldId()].reset(
            common::AttributeConvertorFactory::GetInstance()
            ->CreateAttrConvertor(fieldConfigPtr));
    }
}


// string ExtendDocField2IndexDocField::transIndexTokenizeFieldToSummaryStr(
//         const IndexTokenizeFieldPtr &tokenizeField)
// {
//     string summaryStr;
//     if (!tokenizeField.get()) {
//         return summaryStr;
//     }
    
//     IndexTokenizeField::Iterator it = tokenizeField->createIterator();
//     while (!it.isEnd()) {
//         if ((*it) == NULL) {
//             it.next();
//             continue;
//         }
//         TokenizeSection::Iterator tokenIter = (*it)->createIterator();
//         while (tokenIter) {
//             if (!summaryStr.empty()) {
//                 summaryStr.append("\t");
//             }
//             const ShortString &text = (*tokenIter)->getText();
//             summaryStr.append(text.begin(), text.end());
//             tokenIter.nextBasic();
//         }
//         it.next();
//     }
//     return summaryStr;
// }

IE_NAMESPACE_END(testlib);

