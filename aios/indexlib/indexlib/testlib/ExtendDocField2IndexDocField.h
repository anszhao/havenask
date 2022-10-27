#ifndef __INDEXLIB_EXTENDDOCFIELD2INDEXDOCFIELD_H
#define __INDEXLIB_EXTENDDOCFIELD2INDEXDOCFIELD_H

#include <tr1/memory>
#include <autil/mem_pool/Pool.h>
#include "indexlib/indexlib.h"
#include "indexlib/common_define.h"
#include "indexlib/config/index_partition_schema.h"
#include "indexlib/document/document.h"
#include "indexlib/document/index_document/normal_document/normal_document.h"
#include "indexlib/document/index_document/normal_document/summary_document.h"
#include "indexlib/testlib/raw_document.h"
#include "indexlib/common/field_format/attribute/attribute_convertor.h"
#include "indexlib/partition/remote_access/index_field_convertor.h"

IE_NAMESPACE_BEGIN(testlib);

class ExtendDocField2IndexDocField
{
public:
    ExtendDocField2IndexDocField(const config::IndexPartitionSchemaPtr &schema,
                                 regionid_t regionId = DEFAULT_REGIONID);
    ~ExtendDocField2IndexDocField();
private:
    ExtendDocField2IndexDocField(const ExtendDocField2IndexDocField &);
    ExtendDocField2IndexDocField& operator=(const ExtendDocField2IndexDocField &);
public:
    void convertAttributeField(const document::NormalDocumentPtr &doc,
                               const config::FieldConfigPtr &fieldConfig,
                               const RawDocumentPtr& rawDoc);
    void convertIndexField(const document::NormalDocumentPtr &doc,
                           const config::FieldConfigPtr &fieldConfig, 
                           const RawDocumentPtr& rawDoc);
    void convertSummaryField(const document::SummaryDocumentPtr& doc,
                             const config::FieldConfigPtr &fieldConfig, 
                             const RawDocumentPtr& rawDoc);
    void convertModifyFields(const document::NormalDocumentPtr &doc,
                             const std::string& modifyFields);

    void convertIndexField(const document::IndexDocumentPtr &indexDoc,
                           const config::FieldConfigPtr &fieldConfig,
                           const std::string& fieldStr, const std::string& fieldSep,
                           autil::mem_pool::Pool* pool);
private:
    void init();
    void initAttrConvert();

private:
    typedef common::AttributeConvertorPtr AttributeConvertorPtr;
    typedef std::vector<common::AttributeConvertorPtr> AttributeConvertorVector;

private:
    AttributeConvertorVector mAttrConvertVec;
    config::IndexPartitionSchemaPtr mSchema;
    regionid_t mRegionId;
    partition::IndexFieldConvertor mIndexFieldConvertor;
    
private:
    IE_LOG_DECLARE();
};

DEFINE_SHARED_PTR(ExtendDocField2IndexDocField);

IE_NAMESPACE_END(testlib);

#endif //__INDEXLIB_EXTENDDOCFIELD2INDEXDOCFIELD_H
