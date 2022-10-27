#include "indexlib/index/normal/inverted_index/test/doc_range_partitioner_unittest.h"
#include "indexlib/storage/file_system_wrapper.h"

using namespace std;

IE_NAMESPACE_BEGIN(index);
IE_LOG_SETUP(index, DocRangePartitionerTest);

DocRangePartitionerTest::DocRangePartitionerTest()
{
}

DocRangePartitionerTest::~DocRangePartitionerTest()
{
}

void DocRangePartitionerTest::CaseSetUp()
{
    mRootDir = GET_TEST_DATA_PATH();
    mPartInfoCreator.reset(new PartitionInfoCreator(mRootDir));
}

void DocRangePartitionerTest::CaseTearDown() {
    mPartInfoCreator.reset();
    storage::FileSystemWrapper::DeleteIfExist(mRootDir);
}

void DocRangePartitionerTest::TestSimpleProcess()
{
        {
        CaseTearDown();
        CaseSetUp();
        auto partInfo = mPartInfoCreator->CreatePartitionInfo("100,50,50,70");
        DocIdRangeVector rangeHint { { 30, 80 }, { 110, 130 }, { 230, 250 } };
        // segments : 0, 1, 3
        DocIdRangeVector ranges;

        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 0, ranges));

        EXPECT_EQ(1u, ranges.size());
        EXPECT_EQ(30, ranges[0].first);
        EXPECT_EQ(80, ranges[0].second);
        
        ranges.clear();
        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 1, ranges));
        EXPECT_EQ(2u, ranges.size());
        EXPECT_EQ(110, ranges[0].first);
        EXPECT_EQ(130, ranges[0].second);
        EXPECT_EQ(230, ranges[1].first);
        EXPECT_EQ(250, ranges[1].second);        

        ranges.clear();
        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 3, 1, ranges));
        EXPECT_EQ(2u, ranges.size());
        EXPECT_EQ(60, ranges[0].first);
        EXPECT_EQ(80, ranges[0].second);
        EXPECT_EQ(110, ranges[1].first);
        EXPECT_EQ(120, ranges[1].second);        
    }
    {
        CaseTearDown();
        CaseSetUp();
        auto partInfo = mPartInfoCreator->CreatePartitionInfo("100,50,50,70");
        DocIdRangeVector rangeHint { { 30, 40 }, { 50, 60 }, { 110, 120 }, { 240, 250 } };
        // segments : 0, 1, 3
        DocIdRangeVector ranges;

        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 0, ranges));
        EXPECT_EQ(2u, ranges.size());
        EXPECT_EQ(30, ranges[0].first);
        EXPECT_EQ(40, ranges[0].second);
        EXPECT_EQ(50, ranges[1].first);        
        EXPECT_EQ(60, ranges[1].second);
        ranges.clear();
        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 1, ranges));
        EXPECT_EQ(2u, ranges.size());
        EXPECT_EQ(110, ranges[0].first);
        EXPECT_EQ(120, ranges[0].second);        
        EXPECT_EQ(240, ranges[1].first);
        EXPECT_EQ(250, ranges[1].second);
    }    
    {
        CaseTearDown();
        CaseSetUp();        
        auto partInfo = mPartInfoCreator->CreatePartitionInfo("100,50,50,70");
        DocIdRangeVector rangeHint { { 30, 90 }, { 150, 180 }, { 240, 250 } };
        // segments : 0, 2, 3
        DocIdRangeVector ranges;

        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 0, ranges));

        EXPECT_EQ(1u, ranges.size());
        EXPECT_EQ(30, ranges[0].first);
        EXPECT_EQ(90, ranges[0].second);
        
        ranges.clear();
        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 1, ranges));
        EXPECT_EQ(2u, ranges.size());
        EXPECT_EQ(150, ranges[0].first);
        EXPECT_EQ(180, ranges[0].second);
        EXPECT_EQ(240, ranges[1].first);
        EXPECT_EQ(250, ranges[1].second);
    }    
    {
        CaseTearDown();
        CaseSetUp();        
        auto partInfo = mPartInfoCreator->CreatePartitionInfo("100");
        DocIdRangeVector rangeHint { { 30, 90 } };
        // segments : 0
        DocIdRangeVector ranges;
        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 0, ranges));
        EXPECT_EQ(1u, ranges.size());
        EXPECT_EQ(30, ranges[0].first);
        EXPECT_EQ(60, ranges[0].second);

        ranges.clear();        
        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 1, ranges));
        EXPECT_EQ(1u, ranges.size());
        EXPECT_EQ(60, ranges[0].first);
        EXPECT_EQ(90, ranges[0].second);        
    }
    {
        CaseTearDown();
        CaseSetUp();        
        auto partInfo = mPartInfoCreator->CreatePartitionInfo("100");
        DocIdRangeVector rangeHint { { 30, 940 } };
        // invalid segment
        DocIdRangeVector ranges;
        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 0, ranges));
        EXPECT_EQ(1u, ranges.size());
        EXPECT_EQ(30, ranges[0].first);
        EXPECT_EQ(65, ranges[0].second);
    }
    {
        CaseTearDown();
        CaseSetUp();        
        // realtime range
        auto partInfo = mPartInfoCreator->CreatePartitionInfo("100,20","50,60");
        DocIdRangeVector rangeHint { { 30, 90 }, { 100, 110 }, { 120, 230 } };
        // inc segment: 0, 1, rt segment: 0
        DocIdRangeVector ranges;
        ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 2, 1, ranges));
        EXPECT_EQ(3u, ranges.size());
        EXPECT_EQ(65, ranges[0].first);
        EXPECT_EQ(90, ranges[0].second);
        EXPECT_EQ(100, ranges[1].first);
        EXPECT_EQ(110, ranges[1].second);                
        EXPECT_EQ(120, ranges[2].first);
        EXPECT_EQ(230, ranges[2].second);
    }
}

void DocRangePartitionerTest::TestBatch()
{
    auto partInfo = mPartInfoCreator->CreatePartitionInfo("100,50,50,70", "10,20");
    DocIdRangeVector rangeHint { { 30, 90 }, { 110, 120 }, { 240, 250 }, {270, 271} };
    // segments : 0, 1, 3, rt segment 0, 1
    vector<DocIdRangeVector> rangeVectors;

    ASSERT_TRUE(DocRangePartitioner::GetPartedDocIdRanges(rangeHint, partInfo, 3, rangeVectors));
    EXPECT_EQ(3u, rangeVectors.size());
    auto makeRangeVector = [](std::vector<std::pair<docid_t, docid_t>> ranges) { return ranges; };

    EXPECT_EQ(makeRangeVector({ { 30, 58 } }), rangeVectors[0]);
    EXPECT_EQ(makeRangeVector({ { 58, 90 }  }), rangeVectors[1]);
    EXPECT_EQ(makeRangeVector({ { 110, 120 }, {240, 250},{ 270, 271 } }), rangeVectors[2]);
}

IE_NAMESPACE_END(index);
