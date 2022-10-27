#ifndef __INDEXLIB_PARALLELMERGEINTETEST_H
#define __INDEXLIB_PARALLELMERGEINTETEST_H

#include "indexlib/common_define.h"

#include "indexlib/test/test.h"
#include "indexlib/test/unittest.h"
#include "indexlib/test/partition_state_machine.h"

IE_NAMESPACE_BEGIN(merger);

class ParallelMergeInteTest : public INDEXLIB_TESTBASE
{
public:
    ParallelMergeInteTest();
    ~ParallelMergeInteTest();

    DECLARE_CLASS_NAME(ParallelMergeInteTest);
public:
    void CaseSetUp() override;
    void CaseTearDown() override;
    void TestSimpleProcess();
private:
    IE_LOG_DECLARE();

private:
    std::string mRootDir;
    config::IndexPartitionSchemaPtr mSchema;
    config::IndexPartitionOptions mOptions;
    partition::IndexPartitionResource mPartitionResource;
};

INDEXLIB_UNIT_TEST_CASE(ParallelMergeInteTest, TestSimpleProcess);
IE_NAMESPACE_END(merger);

#endif //__INDEXLIB_PARALLELMERGEINTETEST_H
