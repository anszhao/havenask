#ifndef __INDEXLIB_UPDATEFIELDEXTRACTORTEST_H
#define __INDEXLIB_UPDATEFIELDEXTRACTORTEST_H

#include "indexlib/common_define.h"

#include "indexlib/test/test.h"
#include "indexlib/test/unittest.h"
#include "indexlib/index/normal/attribute/update_field_extractor.h"
#include "indexlib/document/document.h"

DECLARE_REFERENCE_CLASS(document, NormalDocument);

IE_NAMESPACE_BEGIN(index);

class UpdateFieldExtractorTest : public INDEXLIB_TESTBASE
{
public:
    UpdateFieldExtractorTest();
    ~UpdateFieldExtractorTest();

    DECLARE_CLASS_NAME(UpdateFieldExtractorTest);
public:
    void CaseSetUp() override;
    void CaseTearDown() override;
    void TestInit();

private:
    void InnerTestInit(const config::IndexPartitionSchemaPtr& schema, 
                       const document::AttributeDocumentPtr& attrDoc,
                       const std::string& expectFieldInfos);

private:
    config::IndexPartitionSchemaPtr mSchema;
    document::NormalDocumentPtr mDoc;
    std::string mDocStr;

private:
    IE_LOG_DECLARE();
};

INDEXLIB_UNIT_TEST_CASE(UpdateFieldExtractorTest, TestInit);

IE_NAMESPACE_END(index);

#endif //__INDEXLIB_UPDATEFIELDEXTRACTORTEST_H
