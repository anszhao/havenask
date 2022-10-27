#ifndef __INDEXLIB_ONDISKINDEXCLEANERTEST_H
#define __INDEXLIB_ONDISKINDEXCLEANERTEST_H

#include "indexlib/common_define.h"

#include "indexlib/test/test.h"
#include "indexlib/test/unittest.h"
#include "indexlib/partition/on_disk_index_cleaner.h"
#include "indexlib/file_system/indexlib_file_system.h"

IE_NAMESPACE_BEGIN(partition);

class OnDiskIndexCleanerTest : public INDEXLIB_TESTBASE
{
public:
    OnDiskIndexCleanerTest();
    ~OnDiskIndexCleanerTest();

    DECLARE_CLASS_NAME(OnDiskIndexCleanerTest);
public:
    void CaseSetUp() override;
    void CaseTearDown() override;

    void TestSimpleProcess();
    void TestDeployIndexMeta();
    void TestCleanWithNoClean();
    void TestCleanWithInvalidKeepCount();
    void TestCleanWithIncontinuousVersion();
    void TestCleanWithInvalidSegments();
    void TestCleanWithEmptyVersion();
    void TestCleanWithAllEmptyVersion();
    void TestCleanWithUsingVersion();
    void TestCleanWithUsingOldVersion();
    void TestCleanWithRollbackVersion();

private:
    void MakeOnDiskVersion(versionid_t versionId, 
                           const std::string& segmentIds);

private:
    file_system::IndexlibFileSystemPtr mFileSystem;
    file_system::DirectoryPtr mRootDir;

private:
    IE_LOG_DECLARE();
};

INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestSimpleProcess);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestDeployIndexMeta);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestCleanWithNoClean);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestCleanWithInvalidKeepCount);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestCleanWithIncontinuousVersion);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestCleanWithInvalidSegments);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestCleanWithEmptyVersion);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestCleanWithAllEmptyVersion);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestCleanWithUsingVersion);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestCleanWithUsingOldVersion);
INDEXLIB_UNIT_TEST_CASE(OnDiskIndexCleanerTest, TestCleanWithRollbackVersion);

IE_NAMESPACE_END(partition);

#endif //__INDEXLIB_ONDISKINDEXCLEANERTEST_H
