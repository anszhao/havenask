#ifndef __INDEXLIB_PARTITION_RESOURCE_CALCULATOR_H
#define __INDEXLIB_PARTITION_RESOURCE_CALCULATOR_H

#include <tr1/memory>
#include <autil/Lock.h>
#include "indexlib/indexlib.h"
#include "indexlib/common_define.h"
#include "indexlib/config/online_config.h"
#include "indexlib/partition/partition_writer.h"
#include "indexlib/partition/segment/in_memory_segment_container.h"

DECLARE_REFERENCE_CLASS(file_system, Directory);
DECLARE_REFERENCE_CLASS(config, IndexPartitionSchema);
DECLARE_REFERENCE_CLASS(index_base, Version);
DECLARE_REFERENCE_CLASS(plugin, PluginManager);
DECLARE_REFERENCE_CLASS(util, MultiCounter);

IE_NAMESPACE_BEGIN(partition);

class PartitionResourceCalculator
{
public:
    PartitionResourceCalculator(
            const config::OnlineConfig& onlineConfig)
        : mOnlineConfig(onlineConfig)
        , mSwitchRtSegmentLockSize(0)
        , mCurrentIndexSize(0)
    {}

    virtual ~PartitionResourceCalculator() {}

public:
    void Init(const file_system::DirectoryPtr& rootDirectory,
              const PartitionWriterPtr& writer,
              const InMemorySegmentContainerPtr& container,
              const plugin::PluginManagerPtr& pluginManager);
    
    void UpdateSwitchRtSegments(const config::IndexPartitionSchemaPtr& schema,
                                const std::vector<segmentid_t>& segIds);
    
public:
    virtual size_t GetCurrentMemoryUse() const;
    virtual size_t GetRtIndexMemoryUse() const;
    virtual size_t GetWriterMemoryUse() const;
    virtual size_t GetIncIndexMemoryUse() const;
    virtual size_t GetOldInMemorySegmentMemoryUse() const;
    virtual size_t GetBuildingSegmentDumpExpandSize() const;

    size_t EstimateRedoOperationExpandSize(
        const config::IndexPartitionSchemaPtr& schema,
        index_base::PartitionDataPtr& partitinData,
        int64_t timestamp) const;
    void CalculateCurrentIndexSize(const index_base::PartitionDataPtr& partitionData,
                                   const config::IndexPartitionSchemaPtr& schema) const;
    size_t GetCurrentIndexSize() const { return mCurrentIndexSize; }

    void SetWriter(const PartitionWriterPtr& writer)
    {
        autil::ScopedLock lock(mLock);
        assert(!mWriter);
        mWriter = writer;
    }
    
public:
    size_t EstimateDiffVersionLockSizeWithoutPatch(
        const config::IndexPartitionSchemaPtr& schema,
        const file_system::DirectoryPtr& directory,
        const index_base::Version& version,
        const index_base::Version& lastLoadedVersion,
        const util::MultiCounterPtr& counter = util::MultiCounterPtr(),
        bool throwExceptionIfNotExist = true) const;

    size_t EstimateLoadPatchMemoryUse(
        const config::IndexPartitionSchemaPtr& schema,
        const file_system::DirectoryPtr& directory,
        const index_base::Version& version,
        const index_base::Version& lastLoadedVersion) const;

private:
    config::OnlineConfig mOnlineConfig;

    file_system::DirectoryPtr mRootDirectory;
    PartitionWriterPtr mWriter;
    InMemorySegmentContainerPtr mInMemSegContainer;
    plugin::PluginManagerPtr mPluginManager;
    
    std::vector<segmentid_t> mSwitchRtSegIds;
    mutable size_t mSwitchRtSegmentLockSize;
    mutable autil::ThreadMutex mLock;
    mutable size_t mCurrentIndexSize;
    
private:
    friend class PartitionResourceCalculatorTest;
private:
    IE_LOG_DECLARE();
};

DEFINE_SHARED_PTR(PartitionResourceCalculator);

IE_NAMESPACE_END(partition);

#endif //__INDEXLIB_PARTITION_RESOURCE_CALCULATOR_H
