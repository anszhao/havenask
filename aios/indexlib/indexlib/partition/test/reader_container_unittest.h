#ifndef __INDEXLIB_READERCONTAINERTEST_H
#define __INDEXLIB_READERCONTAINERTEST_H

#include "indexlib/common_define.h"

#include "indexlib/test/test.h"
#include "indexlib/test/unittest.h"
#include "indexlib/partition/reader_container.h"

IE_NAMESPACE_BEGIN(partition);

class ReaderContainerTest : public INDEXLIB_TESTBASE
{
public:
    ReaderContainerTest();
    ~ReaderContainerTest();

    DECLARE_CLASS_NAME(ReaderContainerTest);
public:
    void CaseSetUp() override;
    void CaseTearDown() override;

    void TestSimpleProcess();
    void TestHasReader();
    void TestGetSwitchRtSegments();
    
private:
    IE_LOG_DECLARE();
};

INDEXLIB_UNIT_TEST_CASE(ReaderContainerTest, TestSimpleProcess);
INDEXLIB_UNIT_TEST_CASE(ReaderContainerTest, TestHasReader);
INDEXLIB_UNIT_TEST_CASE(ReaderContainerTest, TestGetSwitchRtSegments);

IE_NAMESPACE_END(partition);

#endif //__INDEXLIB_READERCONTAINERTEST_H
