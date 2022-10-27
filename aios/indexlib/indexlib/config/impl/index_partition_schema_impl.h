#ifndef __INDEXLIB_INDEXPARTITIONSCHEMAIMPL_H
#define __INDEXLIB_INDEXPARTITIONSCHEMAIMPL_H

#include <tr1/memory>
#include "indexlib/common_define.h"
#include "indexlib/indexlib.h"
#include "indexlib/config/index_partition_schema.h"
#include "indexlib/config/adaptive_dictionary_schema.h"
#include "indexlib/config/dictionary_schema.h"
#include "indexlib/config/region_schema.h"
#include "indexlib/config/impl/region_schema_impl.h"
#include "indexlib/config/kkv_index_config.h"
#include "indexlib/config/impl/kkv_index_config_impl.h"
#include "indexlib/config/customized_table_config.h"
#include "indexlib/config/customized_config.h"
#include "indexlib/misc/exception.h"
#include "autil/legacy/jsonizable.h"

DECLARE_REFERENCE_CLASS(config, TruncateOptionConfig);
DECLARE_REFERENCE_CLASS(config, IndexPartitionSchema);

IE_NAMESPACE_BEGIN(config);

class IndexPartitionSchemaImpl : public autil::legacy::Jsonizable
{
public:
    IndexPartitionSchemaImpl(const std::string& schemaName = "noname");
    ~IndexPartitionSchemaImpl();

public:
    DictionaryConfigPtr AddDictionaryConfig(const std::string& dictName, 
            const std::string& content);

    FieldConfigPtr AddFieldConfig(const std::string& fieldName, 
                                  FieldType fieldType, 
                                  bool multiValue = false,
                                  bool isVirtual = false,
                                  bool isBinary = false);
    
    EnumFieldConfigPtr AddEnumFieldConfig(const std::string& fieldName, 
            FieldType fieldType, std::vector<std::string>& validValues, 
            bool multiValue = false);

    void AddRegionSchema(const RegionSchemaPtr& regionSchema);
    size_t GetRegionCount() const { return mRegionVector.size(); }
    const RegionSchemaPtr& GetRegionSchema(const std::string& regionName) const;
    const RegionSchemaPtr& GetRegionSchema(regionid_t regionId) const;
    regionid_t GetRegionId(const std::string& regionName) const;
    
    void SetSchemaName(const std::string& schemaName) { mSchemaName = schemaName; }
    const std::string& GetSchemaName() const {return mSchemaName;}

    TableType GetTableType() const;
    void SetTableType(const std::string &str);
    static std::string TableType2Str(TableType tableType);

    const autil::legacy::json::JsonMap &GetUserDefinedParam() const
    { return mUserDefinedParam; }
    autil::legacy::json::JsonMap &GetUserDefinedParam()
    { return mUserDefinedParam; }
    void SetUserDefinedParam(const autil::legacy::json::JsonMap &jsonMap)
    { mUserDefinedParam = jsonMap; }
    bool GetValueFromUserDefinedParam(const std::string &key, std::string &value) const;

    const autil::legacy::json::JsonMap &GetGlobalRegionIndexPreference() const
    { return mGlobalRegionIndexPreference; }
    autil::legacy::json::JsonMap &GetGlobalRegionIndexPreference()
    { return mGlobalRegionIndexPreference; }
    void SetGlobalRegionIndexPreference(const autil::legacy::json::JsonMap &jsonMap)
    { mGlobalRegionIndexPreference = jsonMap; }

    void SetAdaptiveDictSchema(const AdaptiveDictionarySchemaPtr& adaptiveDictSchema)
    { mAdaptiveDictSchema = adaptiveDictSchema;}
    
    const AdaptiveDictionarySchemaPtr& GetAdaptiveDictSchema() const {return mAdaptiveDictSchema;}
    const DictionarySchemaPtr& GetDictSchema() const {return mDictSchema;}
    
    const FieldSchemaPtr& GetFieldSchema(regionid_t regionId) const;
    const IndexSchemaPtr& GetIndexSchema(regionid_t regionId) const;
    const AttributeSchemaPtr& GetAttributeSchema(regionid_t regionId) const;
    const AttributeSchemaPtr& GetVirtualAttributeSchema(regionid_t regionId) const;
    const SummarySchemaPtr& GetSummarySchema(regionid_t regionId) const;
    const TruncateProfileSchemaPtr& GetTruncateProfileSchema(regionid_t regionId) const;
    const FieldSchemaPtr& GetFieldSchema()const
    { return GetFieldSchema(mDefaultRegionId); }
    const IndexSchemaPtr& GetIndexSchema() const
    { return GetIndexSchema(mDefaultRegionId); }
    const AttributeSchemaPtr& GetAttributeSchema() const
    { return GetAttributeSchema(mDefaultRegionId); }
    const AttributeSchemaPtr& GetVirtualAttributeSchema() const
    { return GetVirtualAttributeSchema(mDefaultRegionId); }
    const SummarySchemaPtr& GetSummarySchema() const
    { return GetSummarySchema(mDefaultRegionId); }
    const TruncateProfileSchemaPtr& GetTruncateProfileSchema() const
    { return GetTruncateProfileSchema(mDefaultRegionId); }
    
    const IndexPartitionSchemaPtr& GetSubIndexPartitionSchema() const
    { return mSubIndexPartitionSchema; }

    void Jsonize(autil::legacy::Jsonizable::JsonWrapper& json);
    void AssertEqual(const IndexPartitionSchemaImpl& other) const;

    // only literal clone (rewrite data not cloned)
    IndexPartitionSchemaImpl* Clone() const;

    // shallow clone 
    IndexPartitionSchemaImpl* CloneWithDefaultRegion(regionid_t regionId) const;
    
    void CloneVirtualAttributes(const IndexPartitionSchemaImpl* other);
    bool AddVirtualAttributeConfigs(const AttributeConfigVector& virtualAttrConfigs,
                                    regionid_t id = DEFAULT_REGIONID);

    /**
     * #*this# is compatible with #other# if and only if:
     * (1) all fields and their fieldIds in #*this# must be the same in #other#
     * (2) all attributes field in #*this# must be in #other#
     * (3) all indexes in #*this# must also be in #other#
     * (4) all summary field in #*this# must also be in #other#
     */
    void AssertCompatible(const IndexPartitionSchemaImpl& other) const;

    void Check() const;

    /**
     * when 
     *   mSummarySchema is NULL 
     * or
     *   all summary fields are also in attribute schema
     * do not need store summary
     */
    bool NeedStoreSummary() const;

    bool IsUsefulField(const std::string& fieldName) const;

    void SetSubIndexPartitionSchema(const IndexPartitionSchemaPtr& schema)
    { mSubIndexPartitionSchema = schema; }

    void SetEnableTTL(bool enableTTL, regionid_t id = DEFAULT_REGIONID,
                      const std::string& ttlFieldName = DOC_TIME_TO_LIVE_IN_SECONDS);
    
    bool TTLEnabled(regionid_t id = DEFAULT_REGIONID) const;
    
    void SetDefaultTTL(int64_t defaultTTL, regionid_t id = DEFAULT_REGIONID,
                       const std::string& ttlFieldName = DOC_TIME_TO_LIVE_IN_SECONDS);
    
    int64_t GetDefaultTTL(regionid_t id = DEFAULT_REGIONID) const;

    const std::string& GetTTLFieldName(regionid_t id = DEFAULT_REGIONID);

    void SetAutoUpdatePreference(bool autoUpdate)
    { mAutoUpdatePreference = autoUpdate; }

    bool GetAutoUpdatePreference() const
    { return mAutoUpdatePreference; }

    bool SupportAutoUpdate() const;
    
    void LoadFieldSchema(const autil::legacy::Any& any);
    void LoadFieldSchema(const autil::legacy::Any& any,
                         std::vector<FieldConfigPtr>& fields);

    void SetDefaultRegionId(regionid_t regionId);
    regionid_t GetDefaultRegionId() const { return mDefaultRegionId; }
    
public:
    // for testlib
    void AddIndexConfig(const IndexConfigPtr& indexConfig, regionid_t id = DEFAULT_REGIONID);
    void AddAttributeConfig(const std::string& fieldName, regionid_t id = DEFAULT_REGIONID);
    
    void AddPackAttributeConfig(const std::string& attrName,
                                const std::vector<std::string>& subAttrNames,
                                const std::string& compressTypeStr,
                                regionid_t id = DEFAULT_REGIONID);

    void AddVirtualAttributeConfig(const config::AttributeConfigPtr& virtualAttrConfig,
                                   regionid_t id = DEFAULT_REGIONID);

    void AddSummaryConfig(const std::string& fieldName,
                          summarygroupid_t summaryGroupId = DEFAULT_SUMMARYGROUPID,
                          regionid_t id = DEFAULT_REGIONID);
    void SetTruncateProfileSchema(const TruncateProfileSchemaPtr& truncateProfileSchema,
                                  regionid_t id = DEFAULT_REGIONID);
    void LoadValueConfig(regionid_t id = DEFAULT_REGIONID);

    void ResetRegions();

    const FieldSchemaPtr& GetDefaultFieldSchema() const
    {
        return mFieldSchema;
    }

    schemavid_t GetSchemaVersionId() const { return mSchemaId; }
    void SetSchemaVersionId(schemavid_t schemaId) { mSchemaId = schemaId; }

    void SetCustomizedTableConfig(const CustomizedTableConfigPtr& tableConfig)
    { mCustomizedTableConfig = tableConfig; }

    const CustomizedTableConfigPtr& GetCustomizedTableConfig() const
    { return mCustomizedTableConfig; }

    void SetCustomizedDocumentConfig(const CustomizedConfigVector& documentConfigs)
    { mCustomizedDocumentConfigs = documentConfigs; }

    const CustomizedConfigVector& GetCustomizedDocumentConfigs() const
    { return mCustomizedDocumentConfigs; }

    bool HasModifyOperations() const;
    bool MarkOngoingModifyOperation(schema_opid_t opId);
    const SchemaModifyOperationPtr& GetSchemaModifyOperation(schema_opid_t opId) const;
    size_t GetModifyOperationCount() const;
    std::string GetEffectiveIndexInfo(bool modifiedIndexOnly) const;
    void GetModifyOperationIds(std::vector<schema_opid_t> &ids) const;    
    void GetNotReadyModifyOperationIds(std::vector<schema_opid_t> &ids) const;    
    
    void AddSchemaModifyOperation(const SchemaModifyOperationPtr& op);
    
    void SetBaseSchemaImmutable();
    void SetModifySchemaImmutable();
    void SetModifySchemaMutable();

    void SetMaxModifyOperationCount(uint32_t count) { mMaxModifyOperationCount = count; }
    uint32_t GetMaxModifyOperationCount() const { return mMaxModifyOperationCount; }

    void EnableThrowAssertCompatibleException() { mThrowAssertCompatibleException = true; }
private:
    void DoAssertCompatible(const IndexPartitionSchemaImpl& other) const;
    void CheckFieldSchema() const;    
    void CheckIndexNameNotSummary(const IndexConfigPtr& indexConfig) const;
    void CheckTruncateSortParams() const;
    void CheckSubSchema() const;
    
    IndexPartitionSchemaImpl* CreateBaseSchemaWithoutModifyOperation() const;
    
private:
    typedef std::unordered_map<std::string, size_t> RegionIdMap;
    typedef std::vector<RegionSchemaPtr> RegionVector;
    
private:
    std::string mSchemaName;
    AdaptiveDictionarySchemaPtr mAdaptiveDictSchema;
    DictionarySchemaPtr mDictSchema;
    FieldSchemaPtr mFieldSchema;
    
    RegionIdMap mRegionIdMap;
    RegionVector mRegionVector;

    std::vector<SchemaModifyOperationPtr> mModifyOperations;
    IndexPartitionSchemaPtr mSubIndexPartitionSchema;
    uint32_t mMaxModifyOperationCount;
    bool mAutoUpdatePreference;
    bool mThrowAssertCompatibleException;
    TableType mTableType;
    autil::legacy::json::JsonMap mUserDefinedParam;
    autil::legacy::json::JsonMap mGlobalRegionIndexPreference;
    regionid_t mDefaultRegionId;
    schemavid_t mSchemaId;
    //customized config
    CustomizedTableConfigPtr mCustomizedTableConfig;
    CustomizedConfigVector mCustomizedDocumentConfigs;
    
private:
    friend class IndexPartitionSchemaTest;
    IE_LOG_DECLARE();
};
DEFINE_SHARED_PTR(IndexPartitionSchemaImpl);
IE_NAMESPACE_END(config);

#endif //__INDEXLIB_INDEXPARTITIONSCHEMAIMPL_H
