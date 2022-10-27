#ifndef __INDEXLIB_FAKEINDEXPARTITIONTEST_H
#define __INDEXLIB_FAKEINDEXPARTITIONTEST_H

#include "indexlib/common_define.h"

#include "indexlib/test/test.h"
#include "indexlib/test/unittest.h"
#include "indexlib/testlib/fake_index_partition.h"

IE_NAMESPACE_BEGIN(testlib);

class FakeIndexPartitionTest : public INDEXLIB_TESTBASE
{
public:
    FakeIndexPartitionTest();
    ~FakeIndexPartitionTest();

    DECLARE_CLASS_NAME(FakeIndexPartitionTest);
public:
    void CaseSetUp() override;
    void CaseTearDown() override;
    void TestSimpleProcess();
private:
    IE_LOG_DECLARE();
};

INDEXLIB_UNIT_TEST_CASE(FakeIndexPartitionTest, TestSimpleProcess);

IE_NAMESPACE_END(testlib);

#endif //__INDEXLIB_FAKEINDEXPARTITIONTEST_H
