#include "indexlib/document/document_rewriter/section_attribute_appender.h"
#include "indexlib/document/index_document/normal_document/index_document.h"
#include "indexlib/common/field_format/section_attribute/section_attribute_formatter.h"
#include "indexlib/common/field_format/attribute/string_attribute_convertor.h"
#include "indexlib/config/index_partition_schema.h"
#include <autil/ConstString.h>

using namespace autil;
using namespace std;
IE_NAMESPACE_USE(document);
IE_NAMESPACE_USE(config);
IE_NAMESPACE_USE(common);

IE_NAMESPACE_BEGIN(document);
IE_LOG_SETUP(document, SectionAttributeAppender);

SectionAttributeAppender::SectionAttributeAppender()
    : mSectionsCountInCurrentDoc(0)
    , mTotalSectionLen(0)
    , mSectionOverFlowFlag(false)
{
}

SectionAttributeAppender::SectionAttributeAppender(
        const SectionAttributeAppender& other)
    : mIndexMetaVec(other.mIndexMetaVec)
    , mSectionsCountInCurrentDoc(0)
    , mTotalSectionLen(0)
    , mSectionOverFlowFlag(false)
{
}

SectionAttributeAppender::~SectionAttributeAppender() 
{
}

bool SectionAttributeAppender::Init(const IndexPartitionSchemaPtr& schema)
{
    assert(mIndexMetaVec.empty());
    const IndexSchemaPtr& indexSchema = schema->GetIndexSchema();
    assert(indexSchema);

    auto indexConfigs = indexSchema->CreateIterator(false);
    auto iter = indexConfigs->Begin();
    for (; iter != indexConfigs->End(); iter++)
    {
        IndexType indexType = (*iter)->GetIndexType();
        if (indexType == it_pack || indexType == it_expack)
        {
            PackageIndexConfigPtr packIndexConfig = DYNAMIC_POINTER_CAST(
                    PackageIndexConfig, *iter);
            assert(packIndexConfig);
            if (!packIndexConfig->HasSectionAttribute())
            {
                continue;
            }
	    
            SectionAttributeConfigPtr sectionAttrConf =
                packIndexConfig->GetSectionAttributeConfig();
            assert(sectionAttrConf);
            AttributeConfigPtr attrConfig = sectionAttrConf->CreateAttributeConfig(
                    packIndexConfig->GetIndexName());

            IndexMeta indexMeta;
            indexMeta.packIndexConfig = packIndexConfig;
            indexMeta.formatter.reset(
                    new SectionAttributeFormatter(sectionAttrConf));
            indexMeta.convertor.reset(
                    new StringAttributeConvertor(attrConfig->IsUniqEncode(), 
                            attrConfig->GetAttrName()));
            mIndexMetaVec.push_back(indexMeta);
        }
    }
    return !mIndexMetaVec.empty();
}

bool SectionAttributeAppender::AppendSectionAttribute(
        const IndexDocumentPtr& indexDocument)
{
    assert(indexDocument);
    if (indexDocument->GetMaxIndexIdInSectionAttribute() != INVALID_INDEXID)
    {
        // already append
        return false;
    }

    for (size_t i = 0; i < mIndexMetaVec.size(); ++i)
    {
        AppendSectionAttributeForOneIndex(mIndexMetaVec[i], indexDocument);
        EncodeSectionAttributeForOneIndex(mIndexMetaVec[i], indexDocument);
    }
    return true;
}

void SectionAttributeAppender::AppendSectionAttributeForOneIndex(
        const IndexMeta& indexMeta, const IndexDocumentPtr& indexDocument)
{
    mSectionsCountInCurrentDoc = 0;
    mTotalSectionLen = 0;
    mSectionOverFlowFlag = false;

    int32_t lastFieldIdxInPack = 0;
    SectionAttributeConfigPtr sectionAttrConfig =
        indexMeta.packIndexConfig->GetSectionAttributeConfig();
    bool hasFid = sectionAttrConfig->HasFieldId();
    bool hasSectionWeight = sectionAttrConfig->HasSectionWeight();

    IndexConfig::Iterator iter = indexMeta.packIndexConfig->CreateIterator();
    while (iter.HasNext() && !mSectionOverFlowFlag)
    {
        FieldConfigPtr fieldConfig = iter.Next();
        fieldid_t fieldId = fieldConfig->GetFieldId();
        Field* field = indexDocument->GetField(fieldId);
        if (!field)
        {
            IE_LOG(DEBUG, "Field [%d] not exit in IndexDocument", fieldId);
            continue;
        }
        auto tokenizeField = dynamic_cast<IndexTokenizeField*>(field);
        if (!tokenizeField)
        {
            INDEXLIB_FATAL_ERROR(InconsistentState,
                    "fieldTag [%d] is not IndexTokenizeField",
                    static_cast<int8_t>(field->GetFieldTag()));
        }

        int32_t fieldIdxInPack = indexMeta.packIndexConfig->GetFieldIdxInPack(fieldId);
        if (fieldIdxInPack < 0 ||
            fieldIdxInPack >= (int32_t)PackageIndexConfig::PACK_MAX_FIELD_NUM)
        {
            IE_LOG(WARN, "fieldId in Section overflow (>= %u): fieldId = %d", 
                   PackageIndexConfig::PACK_MAX_FIELD_NUM, fieldIdxInPack);
        }

        AppendSectionAttributeForOneField(tokenizeField, fieldIdxInPack - lastFieldIdxInPack,
                hasFid, hasSectionWeight);
        lastFieldIdxInPack = fieldIdxInPack;
    }

    if (mSectionOverFlowFlag)
    {
        IE_LOG(WARN, "Too many sections in doc(%d).", indexDocument->GetDocId());
    }
}

void SectionAttributeAppender::EncodeSectionAttributeForOneIndex(
        const IndexMeta& indexMeta, const IndexDocumentPtr& indexDocument) 
{
    indexid_t indexId = indexMeta.packIndexConfig->GetIndexId();
    uint8_t buf[SectionAttributeFormatter::DATA_SLICE_LEN];
    uint32_t encodedSize = indexMeta.formatter->EncodeToBuffer(mSectionLens,
            mSectionFids, mSectionWeights, mSectionsCountInCurrentDoc, buf);
    const ConstString sectionData((const char*)buf, (size_t)encodedSize);
    ConstString convertData = indexMeta.convertor->Encode(
            sectionData, indexDocument->GetMemPool());
    indexDocument->SetSectionAttribute(indexId, convertData);
}

void SectionAttributeAppender::AppendSectionAttributeForOneField(
        IndexTokenizeField* field, int32_t dGapFid, bool hasFid, bool hasSectionWeight)
{
    assert(field);
    bool firstSecInField = true;
    auto sectionIter = field->Begin();
    for (; sectionIter != field->End(); ++sectionIter)
    {
        if (mSectionsCountInCurrentDoc == MAX_SECTION_COUNT_PER_DOC)
        {
            mSectionOverFlowFlag = true;
            break;
        }

        if ((*sectionIter)->GetLength() > Section::MAX_SECTION_LENGTH)
        {
            IE_LOG(WARN, "Section Length overflow (> %u): length = %u",
                   Section::MAX_SECTION_LENGTH,  (*sectionIter)->GetLength());
            ERROR_COLLECTOR_LOG(WARN, "Section Length overflow (> %u): length = %u", 
                   Section::MAX_SECTION_LENGTH,  (*sectionIter)->GetLength());
        }

        if (hasFid)
        {
            if (firstSecInField)
            {
                mSectionFids[mSectionsCountInCurrentDoc] = dGapFid;
                firstSecInField = false;
            }
            else
            {
                mSectionFids[mSectionsCountInCurrentDoc] = 0;
            }
        }

        if (hasSectionWeight)
        {
            mSectionWeights[mSectionsCountInCurrentDoc] = (*sectionIter)->GetWeight();
        }
        mSectionLens[mSectionsCountInCurrentDoc] = (*sectionIter)->GetLength();
        mTotalSectionLen += (*sectionIter)->GetLength();
        mSectionsCountInCurrentDoc++;
    }
}

SectionAttributeAppender* SectionAttributeAppender::Clone()
{
    return new SectionAttributeAppender(*this);
}

IE_NAMESPACE_END(document);

