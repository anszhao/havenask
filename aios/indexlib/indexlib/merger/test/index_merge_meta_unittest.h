#ifndef __INDEXLIB_INDEXMERGEMETATEST_H
#define __INDEXLIB_INDEXMERGEMETATEST_H

#include "indexlib/common_define.h"
#include "indexlib/test/test.h"
#include "indexlib/test/unittest.h"
#include "indexlib/merger/index_merge_meta.h"

IE_NAMESPACE_BEGIN(merger);

class IndexMergeMetaTest : public INDEXLIB_TESTBASE
{
public:
    IndexMergeMetaTest();
    ~IndexMergeMetaTest();

    DECLARE_CLASS_NAME(IndexMergeMetaTest);
public:
    void CaseSetUp() override;
    void CaseTearDown() override;
    void TestStoreAndLoad();
    void TestLoadAndStoreWithMergeResource();
    void TestLoadLegacyMeta();

private:
    std::vector<MergeTaskItems> CreateMergeTaskItems(
            uint32_t instanceCount);
protected:
    std::string mRootDir;
private:
    IE_LOG_DECLARE();
};

IE_NAMESPACE_END(merger);

#endif //__INDEXLIB_INDEXMERGEMETATEST_H
