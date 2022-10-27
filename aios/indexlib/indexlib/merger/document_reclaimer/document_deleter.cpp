#include "indexlib/merger/document_reclaimer/document_deleter.h"
#include "indexlib/index/normal/attribute/accessor/attribute_reader_factory.h"

using namespace std;
using namespace autil::mem_pool;
IE_NAMESPACE_USE(config);
IE_NAMESPACE_USE(index);
IE_NAMESPACE_USE(file_system);

IE_NAMESPACE_BEGIN(merger);
IE_LOG_SETUP(merger, DocumentDeleter);

DocumentDeleter::DocumentDeleter(const IndexPartitionSchemaPtr& schema)
    : mPool(new Pool)
    , mSchema(schema)
{
}

DocumentDeleter::~DocumentDeleter() 
{
    // reset writer before reset pool
    mDeletionMapWriter.reset();
    mSubDeletionMapWriter.reset();

    mPartitionData.reset();
}

void DocumentDeleter::Init(const index_base::PartitionDataPtr& partitionData)
{
    mPartitionData = partitionData;
    mDeletionMapWriter.reset(new DeletionMapWriter(true));
    mDeletionMapWriter->Init(partitionData.get());
    if (mSchema->GetSubIndexPartitionSchema())
    {
        mMainJoinAttributeReader = AttributeReaderFactory::CreateJoinAttributeReader(
                mSchema, MAIN_DOCID_TO_SUB_DOCID_ATTR_NAME, partitionData);
        mSubDeletionMapWriter.reset(new DeletionMapWriter(mPool.get()));
        mSubDeletionMapWriter->Init(partitionData->GetSubPartitionData().get());
    }
}

bool DocumentDeleter::Delete(docid_t docId)
{
    assert(mDeletionMapWriter);
    if (mSubDeletionMapWriter)
    {
        docid_t subDocIdBegin = INVALID_DOCID;
        docid_t subDocIdEnd = INVALID_DOCID;
        GetSubDocIdRange(docId, subDocIdBegin, subDocIdEnd);
        // [Begin, end)
        for (docid_t i = subDocIdBegin; i < subDocIdEnd; ++i)
        {
            if (!mSubDeletionMapWriter->Delete(i))
            {
                IE_LOG(ERROR, "failed to delete sub document [%d]", i);
            }
        }
    }
    return mDeletionMapWriter->Delete(docId);
}

void DocumentDeleter::Dump(const file_system::DirectoryPtr& directory)
{
    assert(mDeletionMapWriter);
    mDeletionMapWriter->Dump(directory);
    if (mSubDeletionMapWriter)
    {
        DirectoryPtr subDirectory = directory->MakeDirectory(SUB_SEGMENT_DIR_NAME);
        mSubDeletionMapWriter->Dump(subDirectory);
    }
}

bool DocumentDeleter::IsDirty() const
{
    assert(mDeletionMapWriter);
    return mDeletionMapWriter->IsDirty();
}

uint32_t DocumentDeleter::GetMaxTTL() const
{
    vector<segmentid_t> patchSegIds;
    mDeletionMapWriter->GetPatchedSegmentIds(patchSegIds);
    if (mSubDeletionMapWriter)
    {
        mSubDeletionMapWriter->GetPatchedSegmentIds(patchSegIds);
    }
    
    sort(patchSegIds.begin(), patchSegIds.end());
    vector<segmentid_t>::iterator it =
        unique(patchSegIds.begin(), patchSegIds.end());
    patchSegIds.resize(distance(patchSegIds.begin(),it));
    uint32_t maxTTL = 0;;
    for (size_t i = 0; i < patchSegIds.size(); ++i)
    {
        index_base::SegmentData segData = mPartitionData->GetSegmentData(patchSegIds[i]);
        maxTTL = max(maxTTL, segData.GetSegmentInfo().maxTTL);
    }
    return maxTTL;
}

void DocumentDeleter::GetSubDocIdRange(docid_t mainDocId, docid_t& subDocIdBegin,
                                       docid_t& subDocIdEnd) const
{
    subDocIdBegin = 0;
    if (mainDocId > 0)
    {
        subDocIdBegin = mMainJoinAttributeReader->GetJoinDocId(mainDocId - 1);
    }
    subDocIdEnd = mMainJoinAttributeReader->GetJoinDocId(mainDocId);
    
    if (subDocIdEnd < 0
        || subDocIdBegin < 0
        || (subDocIdBegin > subDocIdEnd))
    {
        INDEXLIB_FATAL_ERROR(IndexCollapsed, 
                             "failed to get join docid [%d, %d), mainDocId [%d]",
                             subDocIdBegin, subDocIdEnd, mainDocId);
    }
}

IE_NAMESPACE_END(merger);

