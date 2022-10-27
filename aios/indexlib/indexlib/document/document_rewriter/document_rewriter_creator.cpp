#include "indexlib/document/document_rewriter/document_rewriter_creator.h"
#include "indexlib/document/document_rewriter/timestamp_document_rewriter.h"
#include "indexlib/document/document_rewriter/section_attribute_rewriter.h"
#include "indexlib/document/document_rewriter/pack_attribute_rewriter.h"
#include "indexlib/document/document_rewriter/sub_doc_add_to_update_document_rewriter.h"
#include "indexlib/document/document_rewriter/add_to_update_document_rewriter.h"
#include "indexlib/document/document_rewriter/delete_to_add_document_rewriter.h"
#include "indexlib/index_define.h"

using namespace std;
IE_NAMESPACE_USE(config);
IE_NAMESPACE_USE(common);
IE_NAMESPACE_USE(index);

IE_NAMESPACE_BEGIN(document);
IE_LOG_SETUP(document, DocumentRewriterCreator);

DocumentRewriterCreator::DocumentRewriterCreator() 
{
}

DocumentRewriterCreator::~DocumentRewriterCreator() 
{
}

DocumentRewriter* DocumentRewriterCreator::CreateTimestampRewriter(
        const IndexPartitionSchemaPtr& schema)
{
    fieldid_t fieldId = schema->GetFieldSchema()->GetFieldId(
                VIRTUAL_TIMESTAMP_FIELD_NAME);
    if (fieldId == INVALID_FIELDID)
    {
        return NULL;
    }
    return new TimestampDocumentRewriter(fieldId);
}

DocumentRewriter* DocumentRewriterCreator::CreateAddToUpdateDocumentRewriter(
        const IndexPartitionSchemaPtr& schema,
        const IndexPartitionOptions& options,
        const SortDescriptions& sortDesc)
{
    vector<IndexPartitionOptions> optionVec;
    vector<SortDescriptions> sortDescVec;

    optionVec.push_back(options);
    sortDescVec.push_back(sortDesc);
    return CreateAddToUpdateDocumentRewriter(schema, optionVec, sortDescVec);
}

DocumentRewriter* DocumentRewriterCreator::CreateAddToUpdateDocumentRewriter(
        const IndexPartitionSchemaPtr& schema,
        const vector<IndexPartitionOptions>& optionsVec,
        const vector<SortDescriptions>& sortDescVec)
{
    if (optionsVec.size() != sortDescVec.size())
    {
        INDEXLIB_FATAL_ERROR(BadParameter, "optionsVec size[%lu] "
                             "not equal with sortDescVec[%lu", 
                             optionsVec.size(), sortDescVec.size());
    }

    IndexPartitionSchemaPtr subSchema = schema->GetSubIndexPartitionSchema();
    if (!subSchema)
    {
        return DoCreateAddToUpdateDocumentRewriter(
                schema, GetTruncateOptionConfigs(optionsVec), sortDescVec);
    }

    IndexSchemaPtr indexSchema = schema->GetIndexSchema();
    bool hasMainPk = indexSchema && indexSchema->HasPrimaryKeyIndex();

    IndexSchemaPtr subIndexSchema = subSchema->GetIndexSchema();
    bool hasSubPk = subIndexSchema && subIndexSchema->HasPrimaryKeyIndex();
    
    if (!hasMainPk && !hasSubPk)
    {
        return NULL;
    } 
    else if (!hasMainPk && hasSubPk)
    {
        IE_LOG(ERROR, "Not support: main pk not exist, but sub pk exist");
        ERROR_COLLECTOR_LOG(ERROR, "Not support: main pk not exist, but sub pk exist");
        return NULL;
    } 
    else if (hasMainPk && !hasSubPk)
    {
        //TODO: sub changed, no rewrite
        return DoCreateAddToUpdateDocumentRewriter(
                schema, GetTruncateOptionConfigs(optionsVec), sortDescVec);
    }

    //TODO: sub schema must has attribute
    AddToUpdateDocumentRewriter* mainRewriter = 
        DoCreateAddToUpdateDocumentRewriter(
                schema, GetTruncateOptionConfigs(optionsVec), sortDescVec);
    assert(mainRewriter);
    //TODO: support sub table truncate
    vector<TruncateOptionConfigPtr> truncOptionConfigs(
            optionsVec.size(), TruncateOptionConfigPtr());
    vector<SortDescriptions> subSortDescVec(optionsVec.size());

    AddToUpdateDocumentRewriter* subRewriter = 
        DoCreateAddToUpdateDocumentRewriter(
                subSchema, truncOptionConfigs, subSortDescVec);
    assert(subRewriter);
    SubDocAddToUpdateDocumentRewriter* subDocRewriter = 
        new SubDocAddToUpdateDocumentRewriter;
    subDocRewriter->Init(schema, mainRewriter, subRewriter);
    return subDocRewriter;
}

DocumentRewriter* DocumentRewriterCreator::CreateSectionAttributeRewriter(
	const IndexPartitionSchemaPtr& schema)
{
    unique_ptr<SectionAttributeRewriter> sectionAttributeRewriter(
	new SectionAttributeRewriter());
    
    if (sectionAttributeRewriter->Init(schema))
    {
	return sectionAttributeRewriter.release();
    }
    return NULL;
}

DocumentRewriter* DocumentRewriterCreator::CreatePackAttributeRewriter(
	const config::IndexPartitionSchemaPtr& schema)
{
    unique_ptr<PackAttributeRewriter> packAttributeRewriter(
	new PackAttributeRewriter());
    
    if (packAttributeRewriter->Init(schema))
    {
	return packAttributeRewriter.release();
    }
    return NULL;
}


AddToUpdateDocumentRewriter* DocumentRewriterCreator::DoCreateAddToUpdateDocumentRewriter(
        const IndexPartitionSchemaPtr& schema,
        const vector<TruncateOptionConfigPtr>& truncateOptionConfigs,
        const vector<SortDescriptions>& sortDescVec)
{
    if (!schema->GetAttributeSchema() ||
        !schema->GetIndexSchema()->HasPrimaryKeyIndex())
    {
        return NULL;
    }

    AddToUpdateDocumentRewriter* rewriter = new AddToUpdateDocumentRewriter;
    rewriter->Init(schema, truncateOptionConfigs, sortDescVec);
    return rewriter;
}

DocumentRewriter* DocumentRewriterCreator::CreateDeleteToAddDocumentRewriter()
{
    return new DeleteToAddDocumentRewriter();
}

vector<TruncateOptionConfigPtr> DocumentRewriterCreator::GetTruncateOptionConfigs(
        const vector<IndexPartitionOptions>& optionVec)
{
    vector<TruncateOptionConfigPtr> truncOptionConfigs;
    for (size_t i = 0; i < optionVec.size(); i++)
    {
        truncOptionConfigs.push_back(optionVec[i].GetMergeConfig().truncateOptionConfig);
    }
    return truncOptionConfigs;
}


IE_NAMESPACE_END(document);

