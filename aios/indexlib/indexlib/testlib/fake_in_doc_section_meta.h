#ifndef __INDEXLIB_FAKE_IN_DOC_SECTION_META_H
#define __INDEXLIB_FAKE_IN_DOC_SECTION_META_H

#include <tr1/memory>
#include "indexlib/indexlib.h"
#include "indexlib/common_define.h"
#include "indexlib/index/normal/inverted_index/accessor/in_doc_section_meta.h"

IE_NAMESPACE_BEGIN(testlib);

class FakeInDocSectionMeta : public index::InDocSectionMeta
{
public:
    FakeInDocSectionMeta();
    ~FakeInDocSectionMeta();
public:
    struct SectionInfo
    {
        section_len_t sectionLength;
        section_weight_t sectionWeight;
    };

    struct DocSectionMeta 
    {
        std::map<int32_t, field_len_t> fieldLength;
        std::map<std::pair<int32_t, sectionid_t>, SectionInfo> fieldAndSecionInfo;
    };
public:
    // common interface
    virtual section_len_t GetSectionLen(int32_t fieldPosition, 
            sectionid_t sectId) const;
    virtual section_len_t GetSectionLenByFieldId(fieldid_t fieldId, 
            sectionid_t sectId) const {
        assert(false);
        return (section_len_t)0;
    }

    virtual section_weight_t GetSectionWeight(int32_t fieldPosition, 
            sectionid_t sectId) const;
    virtual section_weight_t GetSectionWeightByFieldId(fieldid_t fieldId, 
            sectionid_t sectId) const {
        assert(false);
        return (section_weight_t)0;
    }

    virtual field_len_t GetFieldLen(int32_t fieldPosition) const;
    virtual field_len_t GetFieldLenByFieldId(fieldid_t fieldId) const {
        assert(false);
        return (field_len_t)0;
    }

    void SetDocSectionMeta(const DocSectionMeta &);

    virtual void GetSectionMeta(uint32_t idx,section_weight_t& sectionWeight,
				fieldid_t& fieldId,section_len_t& sectionLength) const {}

    virtual uint32_t GetSectionCount() const {
        return _docSectionMeta.fieldAndSecionInfo.size();
    }

    virtual uint32_t GetSectionCountInField(int32_t fieldPosition) const 
    {
      assert(false);
      return 0;
    }

private:
    DocSectionMeta _docSectionMeta;   
private:
    IE_LOG_DECLARE();
};

DEFINE_SHARED_PTR(FakeInDocSectionMeta);

IE_NAMESPACE_END(testlib);

#endif //__INDEXLIB_FAKE_IN_DOC_SECTION_META_H
