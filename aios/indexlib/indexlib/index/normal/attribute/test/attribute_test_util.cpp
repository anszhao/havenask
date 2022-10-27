#include "indexlib/index/normal/attribute/test/attribute_test_util.h"
#include "indexlib/config/schema_configurator.h"
#include "indexlib/config/index_config_creator.h"
#include "indexlib/common/field_format/attribute/attribute_convertor_factory.h"
#include "indexlib/misc/random.h"

using namespace std;
using namespace autil;
IE_NAMESPACE_USE(common);
IE_NAMESPACE_USE(config);

IE_NAMESPACE_BEGIN(index);

///////////////////////////////////////////////////////
// methods of AttributeTestUtil

PackageIndexConfigPtr AttributeTestUtil::MakePackageIndexConfig(
        const string& indexName,
        uint32_t maxFieldCount, 
        uint32_t bigFieldIdBase,
        bool hasSectionAttribute)
{
    FieldSchemaPtr fieldSchema(new FieldSchema(maxFieldCount));
    uint32_t i;
    char buf[8];
    for (i = 0; i < maxFieldCount + bigFieldIdBase; ++i)
    {
        FieldConfigPtr fieldConfig(new FieldConfig());
        snprintf(buf, sizeof(buf), "%d", i);
        fieldConfig->SetFieldName(buf);
        fieldConfig->SetFieldType(ft_text);

        fieldSchema->AddFieldConfig(fieldConfig);
    }

    PackageIndexConfigPtr packIndexConfig(new PackageIndexConfig(indexName, it_pack));
    packIndexConfig->SetFieldSchema(fieldSchema);
    for (i = 0; i < maxFieldCount; ++i)
    {
        snprintf(buf, sizeof(buf), "%d", bigFieldIdBase + i);
        packIndexConfig->AddFieldConfig(buf, 0);
    }
    packIndexConfig->SetHasSectionAttributeFlag(hasSectionAttribute);
    return packIndexConfig;
}

IndexPartitionSchemaPtr AttributeTestUtil::MakeSchemaWithPackageIndexConfig(
        const string& indexName,
        uint32_t maxFieldCount,
	IndexType indexType,
        uint32_t bigFieldIdBase,
        bool hasSectionAttribute)
{
    IndexPartitionSchemaPtr schema(new IndexPartitionSchema);
    uint32_t i;
    char buf[8];
    for (i = 0; i < maxFieldCount + bigFieldIdBase; ++i)
    {
        FieldConfigPtr fieldConfig(new FieldConfig());
        snprintf(buf, sizeof(buf), "%d", i);
	schema->AddFieldConfig(buf, ft_text, false, false);
    }

    PackageIndexConfigPtr packIndexConfig(new PackageIndexConfig(indexName, indexType));
    packIndexConfig->SetFieldSchema(schema->GetFieldSchema());
    for (i = 0; i < maxFieldCount; ++i)
    {
        snprintf(buf, sizeof(buf), "%d", bigFieldIdBase + i);
        packIndexConfig->AddFieldConfig(buf, 0);
    }
    packIndexConfig->SetHasSectionAttributeFlag(hasSectionAttribute);
    schema->AddIndexConfig(packIndexConfig);
    return schema;
}

void AttributeTestUtil::CreateDocCounts(uint32_t segmentCount, 
                                        vector<uint32_t>& docCounts)
{
    docCounts.clear();
    for (uint32_t i = 0; i < segmentCount; ++i)
    {
        uint32_t docCount = misc::dev_urandom() % 20 + 10;
        docCounts.push_back(docCount);
    }
}

void AttributeTestUtil::CreateEmptyDelSets(const vector<uint32_t>& docCounts, 
        vector<set<docid_t> >& delDocIdSets)
{
    delDocIdSets.clear();
    for (size_t i = 0; i < docCounts.size(); ++i)
    {
        delDocIdSets.push_back(set<docid_t>());            
    }
}

void AttributeTestUtil::CreateDelSets(const vector<uint32_t>& docCounts,
                                      vector<set<docid_t> >& delDocIdSets)
{
    delDocIdSets.clear();
    for (size_t i = 0; i < docCounts.size(); ++i)
    {
        uint32_t docCount = docCounts[i];
        set<docid_t> delDocIdSet;
        for (uint32_t j = 0; j < docCount; ++j)
        {
            if (misc::dev_urandom() % 2 == 1)
            {
                delDocIdSet.insert(j);
            }
        }
        delDocIdSets.push_back(delDocIdSet);
    }
}

AttributeConfigPtr AttributeTestUtil::CreateAttrConfig(
        FieldType fieldType, bool isMultiValue, const string& fieldName, 
        fieldid_t fid, bool isCompressValue)
{
    FieldConfigPtr fieldConfig(new FieldConfig(fieldName, 
                    fieldType, isMultiValue));
    fieldConfig->SetFieldId(fid);
    if (isCompressValue)
    {
        string compressStr = "equal";
        fieldConfig->SetCompressType(compressStr);
    }

    AttributeConfigPtr attrConfig(new AttributeConfig);
    attrConfig->Init(fieldConfig);
    attrConfig->SetAttrId(0);
    return attrConfig;
}

AttributeConfigPtr AttributeTestUtil::CreateAttrConfig(
        FieldType fieldType, bool isMultiValue, const string& fieldName,
        fieldid_t fid, const string& compressTypeStr, int32_t fixedLen)
{
    FieldConfigPtr fieldConfig(new FieldConfig(fieldName, 
                    fieldType, isMultiValue));
    fieldConfig->SetFieldId(fid);
    fieldConfig->SetCompressType(compressTypeStr);
    fieldConfig->SetFixedMultiValueCount(fixedLen);

    AttributeConfigPtr attrConfig(new AttributeConfig);
    attrConfig->Init(fieldConfig);
    attrConfig->SetAttrId(0);
    return attrConfig;
}


AttributeConvertorPtr AttributeTestUtil::CreateAttributeConvertor(
        AttributeConfigPtr attrConfig)
{
    return AttributeConvertorPtr(
            AttributeConvertorFactory::GetInstance()
            ->CreateAttrConvertor(attrConfig->GetFieldConfig()));
}

IE_NAMESPACE_END(index);

