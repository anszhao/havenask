#include "indexlib/index_base/patch/partition_patch_meta_creator.h"
#include "indexlib/file_system/directory.h"
#include "indexlib/index_base/patch/partition_patch_index_accessor.h"
#include "indexlib/util/path_util.h"
#include "indexlib/index_base/schema_adapter.h"
#include "indexlib/index_base/version_loader.h"
#include "indexlib/index_define.h"
#include "indexlib/storage/file_system_wrapper.h"
#include "indexlib/misc/exception.h"
#include "indexlib/config/index_partition_schema.h"

using namespace std;
using namespace autil;
using namespace autil::legacy;
using namespace fslib;

IE_NAMESPACE_USE(config);
IE_NAMESPACE_USE(file_system);
IE_NAMESPACE_USE(storage);
IE_NAMESPACE_USE(index);
IE_NAMESPACE_USE(util);
IE_NAMESPACE_USE(misc);

IE_NAMESPACE_BEGIN(index_base);
IE_LOG_SETUP(index_base, PartitionPatchMetaCreator);

PartitionPatchMetaCreator::PartitionPatchMetaCreator() 
{
}

PartitionPatchMetaCreator::~PartitionPatchMetaCreator() 
{
}

PartitionPatchMetaPtr PartitionPatchMetaCreator::Create(const DirectoryPtr& rootDir,
        const IndexPartitionSchemaPtr& schema, const Version& version)
{
    if (rootDir->GetStorageType() != FSST_LOCAL)
    {
        INDEXLIB_FATAL_ERROR(UnSupported,
                             "only support create PartitionPatchMeta in local storage dir!");
    }
    return Create(rootDir->GetPath(), schema, version);
}

PartitionPatchMetaPtr PartitionPatchMetaCreator::Create(const string& rootPath,
        const IndexPartitionSchemaPtr& schema, const Version& version)
{
    if (schema->GetSchemaVersionId() != version.GetSchemaVersionId())
    {
        INDEXLIB_FATAL_ERROR(InconsistentState,
                             "schema version not match in schema [%d] and version [%d]",
                             schema->GetSchemaVersionId(),
                             version.GetSchemaVersionId());
    }

    if (schema->GetSchemaVersionId() == DEFAULT_SCHEMAID)
    {
        return PartitionPatchMetaPtr();
    }

    if (schema->HasModifyOperations())
    {
        return CreatePatchMetaForModifySchema(rootPath, schema, version);
    }
    
    schemavid_t lastSchemaId = DEFAULT_SCHEMAID;
    PartitionPatchMetaPtr lastPatchMeta = GetLastPartitionPatchMeta(rootPath, lastSchemaId);
    if (lastSchemaId > schema->GetSchemaVersionId())
    {
        INDEXLIB_FATAL_ERROR(UnSupported,
                             "not support create PartitionPatchMeta "
                             "when last version schema id [%d] greater than current [%d],"
                             "may be just rollback", lastSchemaId, schema->GetSchemaVersionId());
    }
    return CreatePatchMeta(rootPath, schema, version, lastPatchMeta, lastSchemaId);
}
    
PartitionPatchMetaPtr PartitionPatchMetaCreator::GetLastPartitionPatchMeta(
        const string& rootPath, schemavid_t& lastSchemaId)
{
    Version lastVersion;
    VersionLoader::GetVersion(rootPath, lastVersion, INVALID_VERSION);
    if (lastVersion.GetVersionId() == INVALID_VERSION ||
        lastVersion.GetSchemaVersionId() == DEFAULT_SCHEMAID)
    {
        lastSchemaId = DEFAULT_SCHEMAID;
        return PartitionPatchMetaPtr();
    }

    PartitionPatchMetaPtr patchMeta(new PartitionPatchMeta);
    try
    {
        patchMeta->Load(rootPath, lastVersion.GetVersionId());
    }
    catch (const ExceptionBase& e)
    {
        IE_LOG(ERROR, "Exception occurred [%s]", e.what());
        lastSchemaId = DEFAULT_SCHEMAID;
        return PartitionPatchMetaPtr();
    }
    lastSchemaId = lastVersion.GetSchemaVersionId();
    return patchMeta;
}

void PartitionPatchMetaCreator::GetPatchIndexInfos(
        const string& segPath, const IndexPartitionSchemaPtr& schema,
        vector<string>& indexNames, vector<string>& attrNames)
{
    vector<string> backupIndexs;
    string indexPath = PathUtil::JoinPath(segPath, INDEX_DIR_NAME);
    if (FileSystemWrapper::IsExist(indexPath))
    {
        FileList fileList;
        FileSystemWrapper::ListDir(indexPath, fileList);
        backupIndexs.assign(fileList.begin(), fileList.end());
    }

    vector<string> backupAttributes;
    string attrPath = PathUtil::JoinPath(segPath, ATTRIBUTE_DIR_NAME);
    if (FileSystemWrapper::IsExist(attrPath))
    {
        FileList fileList;
        FileSystemWrapper::ListDir(attrPath, fileList);
        backupAttributes.assign(fileList.begin(), fileList.end());
    }
    GetValidIndexAndAttribute(schema, backupIndexs, backupAttributes,
                              indexNames, attrNames);
}

void PartitionPatchMetaCreator::GetValidIndexAndAttribute(
        const IndexPartitionSchemaPtr& schema,
        const vector<string>& inputIndexNames, const vector<string>& inputAttrNames,
        vector<string>& indexNames, vector<string>& attrNames)
{
    IndexSchemaPtr indexSchema = schema->GetIndexSchema();
    if (indexSchema)
    {
        for (size_t i = 0; i < inputIndexNames.size(); i++)
        {
            IndexConfigPtr indexConfig = indexSchema->GetIndexConfig(inputIndexNames[i]);
            if (!indexConfig)
            {
                continue;
            }
            indexNames.push_back(indexConfig->GetIndexName());
            if (indexConfig->GetShardingType() == IndexConfig::IST_IS_SHARDING)
            {
                string indexName;
                if (!IndexConfig::GetIndexNameFromShardingIndexName(
                                indexConfig->GetIndexName(), indexName))
                {
                    INDEXLIB_FATAL_ERROR(Schema,
                            "sharding index config [%s] get parent index name failed",
                            indexConfig->GetIndexName().c_str());
                }
                indexNames.push_back(indexName);
            }
        }
    }
    AttributeSchemaPtr attrSchema = schema->GetAttributeSchema();
    if (attrSchema)
    {
        for (size_t i = 0; i < inputAttrNames.size(); i++)
        {
            AttributeConfigPtr attrConf = attrSchema->GetAttributeConfig(inputAttrNames[i]);
            if (attrConf)
            {
                attrNames.push_back(attrConf->GetAttrName());
            }
        }
    }
}

bool ReverseCompareSchemaId(schemavid_t lft, schemavid_t rht) { return lft > rht; }

PartitionPatchMetaPtr PartitionPatchMetaCreator::CreatePatchMeta(
            const string& rootPath, const IndexPartitionSchemaPtr& schema,
            const Version& version, const PartitionPatchMetaPtr& lastPatchMeta,
            schemavid_t lastSchemaId)
{
    vector<schemavid_t> schemaIdVec;
    FileList patchRootList;
    PartitionPatchIndexAccessor::ListPatchRootDirs(rootPath, patchRootList);
    for (size_t i = 0; i < patchRootList.size(); i++)
    {
        schemavid_t schemaId;
        PartitionPatchIndexAccessor::ExtractSchemaIdFromPatchRootDir(
                patchRootList[i], schemaId);
        if (schemaId > lastSchemaId && schemaId <= schema->GetSchemaVersionId())
        {
            schemaIdVec.push_back(schemaId);
        }
    }
    sort(schemaIdVec.begin(), schemaIdVec.end(), ReverseCompareSchemaId);
    return CreatePatchMeta(rootPath, schema, version, schemaIdVec, lastPatchMeta);
}

PartitionPatchMetaPtr PartitionPatchMetaCreator::CreatePatchMeta(
        const string& rootPath, const IndexPartitionSchemaPtr& schema,
        const Version& version, const vector<schemavid_t>& schemaIdVec,
        const PartitionPatchMetaPtr& lastPatchMeta)
{
    PartitionPatchMetaPtr patchMeta(new PartitionPatchMeta);
    for (size_t i = 0; i < schemaIdVec.size(); i++)
    {
        string patchPath = PathUtil::JoinPath(rootPath,
                PartitionPatchIndexAccessor::GetPatchRootDirName(schemaIdVec[i]));
        Version::Iterator vIter = version.CreateIterator();
        while (vIter.HasNext())
        {
            segmentid_t segId = vIter.Next();
            string segPath = PathUtil::JoinPath(patchPath, version.GetSegmentDirName(segId));
            if (!FileSystemWrapper::IsExist(segPath))
            {
                continue;
            }
            
            vector<string> indexNames;
            vector<string> attrNames;
            GetPatchIndexInfos(segPath, schema, indexNames, attrNames);
            patchMeta->AddPatchIndexs(schemaIdVec[i], segId, indexNames);
            patchMeta->AddPatchAttributes(schemaIdVec[i], segId, attrNames);            
        }
    }

    if (lastPatchMeta)
    {
        PartitionPatchMeta::Iterator iter = lastPatchMeta->CreateIterator();
        while (iter.HasNext())
        {
            SchemaPatchInfoPtr patchInfo = iter.Next();
            schemavid_t schemaId = patchInfo->GetSchemaId();
            assert(schemaId != DEFAULT_SCHEMAID);
            SchemaPatchInfo::Iterator sIter = patchInfo->Begin();
            for (; sIter != patchInfo->End(); sIter++)
            {
                const SchemaPatchInfo::PatchSegmentMeta& segMeta = *sIter;
                segmentid_t segId = segMeta.GetSegmentId();
                if (!version.HasSegment(segId))
                {
                    continue;
                }
                vector<string> indexNames;
                vector<string> attrNames;
                GetValidIndexAndAttribute(schema, segMeta.GetPatchIndexs(),
                        segMeta.GetPatchAttributes(), indexNames, attrNames);
                patchMeta->AddPatchIndexs(schemaId, segId, indexNames);
                patchMeta->AddPatchAttributes(schemaId, segId, attrNames);            
            }
        }
    }
    return patchMeta;
}

PartitionPatchMetaPtr PartitionPatchMetaCreator::CreatePatchMetaForModifySchema(
        const string& rootPath, const IndexPartitionSchemaPtr& schema,
        const Version& version)
{
    if (schema->GetSchemaVersionId() != version.GetSchemaVersionId())
    {
        INDEXLIB_FATAL_ERROR(InconsistentState,
                             "schema id [%d] in version not match with schema [%d]",
                             version.GetSchemaVersionId(),
                             schema->GetSchemaVersionId());
    }
    
    vector<schema_opid_t> notReadyOpIds = version.GetOngoingModifyOperations();
    sort(notReadyOpIds.begin(), notReadyOpIds.end());
    
    vector<schema_opid_t> opIds;
    schema->GetModifyOperationIds(opIds);
    sort(opIds.begin(), opIds.end());

    vector<schema_opid_t> readyOpIds;
    set_difference(opIds.begin(), opIds.end(), notReadyOpIds.begin(), notReadyOpIds.end(),
                   inserter(readyOpIds, readyOpIds.begin()));
    sort(readyOpIds.begin(), readyOpIds.end());
    PartitionPatchMetaPtr patchMeta(new PartitionPatchMeta);
    for (auto id : readyOpIds)
    {
        vector<string> indexNames;
        vector<string> attrNames;
        if (!GetValidIndexAndAttributeForModifyOperation(schema, id, indexNames, attrNames))
        {
            continue;
        }
        // maxOpId equal to schemaId
        string patchRootDirPath = PathUtil::JoinPath(rootPath,
                PartitionPatchIndexAccessor::GetPatchRootDirName(id));
        if (!FileSystemWrapper::IsExist(patchRootDirPath))
        {
            IE_LOG(INFO, "index patch dir [%s] for modify opId [%u] not exist.",
                   patchRootDirPath.c_str(), id);
            continue;
        }
        Version::Iterator vIter = version.CreateIterator();
        while (vIter.HasNext())
        {
            segmentid_t segId = vIter.Next();
            string segPath = PathUtil::JoinPath(patchRootDirPath, version.GetSegmentDirName(segId));
            if (!FileSystemWrapper::IsExist(segPath))
            {
                continue;
            }
            patchMeta->AddPatchIndexs(id, segId, indexNames);
            patchMeta->AddPatchAttributes(id, segId, attrNames);            
        }
    }
    return patchMeta;
}

bool PartitionPatchMetaCreator::GetValidIndexAndAttributeForModifyOperation(
        const IndexPartitionSchemaPtr& schema, schema_opid_t opId,
        vector<string>& indexNames, vector<string>& attrNames)
{
    ModifyItemVector indexs;
    ModifyItemVector attrs;
    SchemaModifyOperationPtr modifyOp = schema->GetSchemaModifyOperation(opId);
    assert(modifyOp);
    modifyOp->CollectEffectiveModifyItem(indexs, attrs);
    if (indexs.empty() && attrs.empty())
    {
        IE_LOG(INFO, "modify opId [%u] not has effective new indexs or attributes.", opId);
        return false;
    }
    for (auto &item: indexs)
    {
        indexNames.push_back(item->GetName());
    }
    for (auto &item: attrs)
    {
        attrNames.push_back(item->GetName());
    }
    return true;
}
    

IE_NAMESPACE_END(index_base);

