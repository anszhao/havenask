#ifndef __INDEXLIB_SECTIONATTRIBUTEFORMATTERTEST_H
#define __INDEXLIB_SECTIONATTRIBUTEFORMATTERTEST_H

#include "indexlib/common_define.h"

#include "indexlib/test/test.h"
#include "indexlib/test/unittest.h"
#include "indexlib/common/field_format/section_attribute/section_attribute_formatter.h"

IE_NAMESPACE_BEGIN(common);

class SectionAttributeFormatterTest : public INDEXLIB_TESTBASE {
public:
    SectionAttributeFormatterTest();
    ~SectionAttributeFormatterTest();
public:
    void SetUp();
    void TearDown();
    void TestSimpleProcess();
    void TestUnpackBuffer();

private:
    SectionAttributeFormatterPtr CreateFormatter(
            bool hasFieldId, bool hasWeight);

    void CheckBuffer(uint8_t *buffer, uint32_t sectionCount,
                     const section_len_t* lenBuffer, 
                     const section_fid_t* fidBuffer, 
                     const section_weight_t* weightBuffer);

private:
    IE_LOG_DECLARE();
};

INDEXLIB_UNIT_TEST_CASE(SectionAttributeFormatterTest, TestSimpleProcess);
INDEXLIB_UNIT_TEST_CASE(SectionAttributeFormatterTest, TestUnpackBuffer);

IE_NAMESPACE_END(common);

#endif //__INDEXLIB_SECTIONATTRIBUTEFORMATTERTEST_H
