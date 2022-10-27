#ifndef __INDEXLIB_MULTIPARTSEGMENTDIRECTORYTEST_H
#define __INDEXLIB_MULTIPARTSEGMENTDIRECTORYTEST_H

#include "indexlib/common_define.h"

#include "indexlib/test/test.h"
#include "indexlib/test/unittest.h"
#include "indexlib/index_base/segment/multi_part_segment_directory.h"

IE_NAMESPACE_BEGIN(index_base);

class MultiPartSegmentDirectoryTest : public INDEXLIB_TESTBASE
{
public:
    MultiPartSegmentDirectoryTest();
    ~MultiPartSegmentDirectoryTest();

    DECLARE_CLASS_NAME(MultiPartSegmentDirectoryTest);
public:
    void CaseSetUp() override;
    void CaseTearDown() override;
    void TestSimpleProcess();
    void TestEncodeToVirtualSegmentId();
private:
    IE_LOG_DECLARE();
};

INDEXLIB_UNIT_TEST_CASE(MultiPartSegmentDirectoryTest, TestSimpleProcess);
INDEXLIB_UNIT_TEST_CASE(MultiPartSegmentDirectoryTest, TestEncodeToVirtualSegmentId);

IE_NAMESPACE_END(index_base);

#endif //__INDEXLIB_MULTIPARTSEGMENTDIRECTORYTEST_H
