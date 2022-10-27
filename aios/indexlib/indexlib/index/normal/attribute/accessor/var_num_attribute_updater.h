#ifndef __INDEXLIB_VAR_NUM_ATTRIBUTE_UPDATER_H
#define __INDEXLIB_VAR_NUM_ATTRIBUTE_UPDATER_H

#include <tr1/memory>
#include <autil/StringUtil.h>
#include <unordered_map>
#include <autil/mem_pool/pool_allocator.h>
#include "indexlib/indexlib.h"
#include "indexlib/common_define.h"
#include "indexlib/common/field_format/attribute/type_info.h"
#include "indexlib/index_define.h"
#include "indexlib/index/normal/attribute/accessor/attribute_updater.h"
#include "indexlib/index/normal/attribute/accessor/attribute_updater_creator.h"
#include "indexlib/config/pack_attribute_config.h"

IE_NAMESPACE_BEGIN(index);

template<typename T>
class VarNumAttributeUpdater : public AttributeUpdater
{
private:
    typedef autil::mem_pool::pool_allocator<std::pair<const docid_t, std::string> > AllocatorType;
    typedef std::unordered_map<docid_t, std::string,
                               std::hash<docid_t>,
                               std::equal_to<docid_t>,
                               AllocatorType> HashMap;       

public:
    VarNumAttributeUpdater(util::BuildResourceMetrics* buildResourceMetrics,
                           segmentid_t segId,
                           const config::AttributeConfigPtr& attrConfig);
    ~VarNumAttributeUpdater() {}

public:
    class Creator : public AttributeUpdaterCreator
    {
    public:
        FieldType GetAttributeType() const
        {
            return common::TypeInfo<T>::GetFieldType();
        }

        AttributeUpdater* Create(
                util::BuildResourceMetrics* buildResourceMetrics,
                segmentid_t segId,
                const config::AttributeConfigPtr& attrConfig) const
        {
            return new VarNumAttributeUpdater<T>(buildResourceMetrics, segId, attrConfig);
        }
    };

public:
    void Update(docid_t docId,
                              const autil::ConstString& attributeValue) override;

    void Dump(const file_system::DirectoryPtr& attributeDir, 
                            segmentid_t segmentId = INVALID_SEGMENTID) override;

private:
    void UpdateBuildResourceMetrics()
    {
        if (!mBuildResourceMetricsNode)
        {
            return;
        }
        int64_t totalMemUse = mSimplePool.getUsedBytes() + mDumpValueSize;
        int64_t docIdMemSize = mHashMap.size() * sizeof(docid_t);
        int64_t dumpTempMemUse = docIdMemSize + GetPatchFileWriterBufferSize();
        int64_t dumpFileSize = EstimateDumpFileSize(docIdMemSize + mDumpValueSize);
        
        mBuildResourceMetricsNode->Update(
                util::BMT_CURRENT_MEMORY_USE, totalMemUse);
        mBuildResourceMetricsNode->Update(
                util::BMT_DUMP_TEMP_MEMORY_SIZE, dumpTempMemUse);
        mBuildResourceMetricsNode->Update(
                util::BMT_DUMP_FILE_SIZE, dumpFileSize);        
    }    
    
private:
    const static size_t MEMORY_USE_ESTIMATE_FACTOR = 2;
    size_t mDumpValueSize;
    HashMap mHashMap;

private:
    IE_LOG_DECLARE();
};

IE_LOG_SETUP_TEMPLATE(index, VarNumAttributeUpdater);

template<typename T>
VarNumAttributeUpdater<T>::VarNumAttributeUpdater(
        util::BuildResourceMetrics* buildResourceMetrics,
        segmentid_t segId,
        const config::AttributeConfigPtr& attrConfig)
    : AttributeUpdater(buildResourceMetrics, segId, attrConfig)
    , mDumpValueSize(0)
    , mHashMap(AllocatorType(&mSimplePool))
{
}

template<typename T>
void VarNumAttributeUpdater<T>::Update(docid_t docId,
        const autil::ConstString& attributeValue)
{
    std::string value = std::string(attributeValue.data(), 
                                    attributeValue.size());
    std::pair<HashMap::iterator, bool> ret = 
        mHashMap.insert(std::make_pair(docId, value));
    if (!ret.second)
    {
        HashMap::iterator& iter = ret.first;
        mDumpValueSize -= iter->second.size();
        iter->second = value;
    }

    mDumpValueSize += value.size();
    UpdateBuildResourceMetrics();
}

template<typename T>
void VarNumAttributeUpdater<T>::Dump(
        const file_system::DirectoryPtr& attributeDir, segmentid_t srcSegment)
{
    std::vector<docid_t> docIdVect;
    if (mHashMap.empty())
    {
        return;
    }

    docIdVect.reserve(mHashMap.size());
    typename HashMap::iterator it = mHashMap.begin();
    typename HashMap::iterator itEnd = mHashMap.end();
    for (; it != itEnd; ++it)
    {
        docIdVect.push_back(it->first);
    }
    std::sort(docIdVect.begin(), docIdVect.end());

    config::PackAttributeConfig* packAttrConfig =
        mAttrConfig->GetPackAttributeConfig();
    std::string attrDir =
        packAttrConfig != NULL ?
        packAttrConfig->GetAttrName() + "/" + mAttrConfig->GetAttrName() :
        mAttrConfig->GetAttrName();

    file_system::DirectoryPtr dir = attributeDir->MakeDirectory(attrDir);

    std::string patchFileName = GetPatchFileName(srcSegment);
    file_system::FileWriterPtr patchFileWriter = 
        CreatePatchFileWriter(dir, patchFileName);

    size_t reserveSize = mDumpValueSize + docIdVect.size() * sizeof(docid_t) + 
                         sizeof(uint32_t) * 2; // patchCount + maxPatchDataLen
    patchFileWriter->ReserveFileNode(reserveSize);

    std::string filePath = patchFileWriter->GetPath();
    IE_LOG(DEBUG, "Begin dumping attribute patch to file : %s",
           filePath.c_str());

    uint32_t maxPatchDataLen = 0;
    uint32_t patchDataCount = mHashMap.size();
    for (size_t i = 0; i < docIdVect.size(); ++i)
    {
        docid_t docId = docIdVect[i];
        std::string str = mHashMap[docId];
        patchFileWriter->Write(&docId, sizeof(docid_t));
        patchFileWriter->Write(str.c_str(), str.size());
        maxPatchDataLen = std::max(maxPatchDataLen, (uint32_t)str.size());
    }
    patchFileWriter->Write(&patchDataCount, sizeof(uint32_t));
    patchFileWriter->Write(&maxPatchDataLen, sizeof(uint32_t));

    assert(mAttrConfig->GetCompressType().HasPatchCompress() ||
           patchFileWriter->GetLength() == reserveSize);
    patchFileWriter->Close();
    IE_LOG(DEBUG, "Finish dumping attribute patch to file : %s",
           filePath.c_str());
}

typedef VarNumAttributeUpdater<int8_t> Int8MultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<uint8_t> UInt8MultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<int16_t> Int16MultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<uint16_t> UInt16MultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<int32_t> Int32MultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<uint32_t> UInt32MultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<int64_t> Int64MultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<uint64_t> UInt64MultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<float> FloatMultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<double> DoubleMultiValueAttributeUpdater;
typedef VarNumAttributeUpdater<autil::MultiChar> MultiStringAttributeUpdater;

IE_NAMESPACE_END(index);

#endif //__INDEXLIB_VAR_NUM_ATTRIBUTE_UPDATER_H
