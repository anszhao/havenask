#ifndef __INDEXLIB_BUILDING_RANGE_INDEX_READER_H
#define __INDEXLIB_BUILDING_RANGE_INDEX_READER_H

#include <tr1/memory>
#include "indexlib/indexlib.h"

#include "indexlib/common_define.h"
#include "indexlib/index/normal/inverted_index/accessor/building_index_reader.h"
#include "indexlib/index/normal/inverted_index/accessor/segment_postings.h"

IE_NAMESPACE_BEGIN(index);

class BuildingRangeIndexReader : public index::BuildingIndexReader
{
public:
    BuildingRangeIndexReader(const index::PostingFormatOption& postingFormatOption);
    ~BuildingRangeIndexReader();
public:
    //todo extrace date segment posting vec
    void Lookup(uint64_t leftTerm, uint64_t rightTerm, SegmentPostingsVec& segmentPostings,
                autil::mem_pool::Pool *sessionPool);

private:
    IE_LOG_DECLARE();
};

DEFINE_SHARED_PTR(BuildingRangeIndexReader);

IE_NAMESPACE_END(index);

#endif //__INDEXLIB_BUILDING_RANGE_INDEX_READER_H
