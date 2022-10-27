#include "indexlib/config/impl/attribute_schema_impl.h"
#include "autil/legacy/any.h"
#include "autil/legacy/jsonizable.h"
#include "autil/legacy/json.h"
#include "indexlib/misc/exception.h"
#include "indexlib/config/field_config.h"
#include "indexlib/config/config_define.h"
#include "indexlib/config/configurator_define.h"

IE_NAMESPACE_USE(misc);
using namespace std;
using namespace autil::legacy;
using namespace autil::legacy::json;

IE_NAMESPACE_BEGIN(config);
IE_LOG_SETUP(config, AttributeSchemaImpl);

AttributeConfigPtr AttributeSchemaImpl::NULL_ATTR_CONF = AttributeConfigPtr();
PackAttributeConfigPtr AttributeSchemaImpl::NULL_PACK_ATTR_CONF = PackAttributeConfigPtr();

AttributeSchemaImpl::AttributeSchemaImpl() 
    : mMaxFieldId(INVALID_FIELDID)
    , mBaseAttrCount(-1)
    , mOnlyAddVirtual(false)
{
}

AttributeSchemaImpl::~AttributeSchemaImpl() 
{
}

const AttributeConfigPtr& AttributeSchemaImpl::GetAttributeConfig(
        const string& attrName) const 
{
    attrid_t attrId = GetAttributeId(attrName);
    return GetAttributeConfig(attrId);
}

const AttributeSchemaImpl::AttrVector& AttributeSchemaImpl::GetAttributeConfigs() const
{
    return mAttrConfigs;
}

void AttributeSchemaImpl::AddAttributeConfig(const AttributeConfigPtr& attrConfig)
{
    assert(attrConfig);
    AddAttributeConfig(attrConfig, mAttrName2IdMap, mAttrConfigs);
}

void AttributeSchemaImpl::AddPackAttributeConfig(
    const PackAttributeConfigPtr& packAttrConfig)
{
    if (GetAttributeConfig(packAttrConfig->GetAttrName()) != NULL_ATTR_CONF)
    {
        INDEXLIB_FATAL_ERROR(Schema,
                       "Pack Attribute Name is duplicate with normal attribute: %s",
                       packAttrConfig->GetAttrName().c_str());
    }

    NameMap::iterator it =
        mPackAttrName2IdMap.find(packAttrConfig->GetAttrName());
    if (it != mPackAttrName2IdMap.end())
    {
        INDEXLIB_FATAL_ERROR(Schema, "Duplicate Pack Attribute Name: %s",
                       packAttrConfig->GetAttrName().c_str());
    }

    const vector<AttributeConfigPtr>& attrConfigs =
        packAttrConfig->GetAttributeConfigVec();
    for (size_t i = 0; i < attrConfigs.size(); i++)
    {
        AddAttributeConfig(attrConfigs[i]);
    }
    packAttrConfig->SetPackAttrId(mPackAttrConfigs.size());
    mPackAttrName2IdMap[packAttrConfig->GetAttrName()] = mPackAttrConfigs.size();
    mPackAttrConfigs.push_back(packAttrConfig);
}

void AttributeSchemaImpl::AddAttributeConfig(const AttributeConfigPtr& attrConfig,
                                             NameMap& nameMap, AttrVector& attrConfigs)
{
    if (mOnlyAddVirtual)
    {
        INDEXLIB_FATAL_ERROR(UnSupported,
                             "add attribute [%s] fail, only support add virtual attribute",
                             attrConfig->GetAttrName().c_str());
    }
    NameMap::iterator it = nameMap.find(attrConfig->GetAttrName());
    if (it != nameMap.end()) 
    {
        INDEXLIB_FATAL_ERROR(Schema, "Duplicate Attribute Name: %s", 
                       attrConfig->GetAttrName().c_str());
    }
    attrConfig->SetAttrId(mAttrConfigs.size());
    attrConfigs.push_back(attrConfig);
    nameMap.insert(make_pair(attrConfig->GetAttrName(),
                             attrConfig->GetAttrId()));

    fieldid_t fieldId = attrConfig->GetFieldConfig()->GetFieldId();
    if (fieldId != INVALID_FIELDID)
    {
        SetExistAttrConfig(fieldId, attrConfig);
    }
}

void AttributeSchemaImpl::SetExistAttrConfig(
    fieldid_t fieldId, const AttributeConfigPtr& attrConf)
{
    assert(attrConf);
    assert(fieldId != INVALID_FIELDID);
    if (fieldId > mMaxFieldId)
    {
        mFieldId2AttrConfigVec.resize(fieldId + 1, AttributeConfigPtr());
        mMaxFieldId = fieldId;
    }
    mFieldId2AttrConfigVec[fieldId] = attrConf;
}

void AttributeSchemaImpl::AssertEqual(const AttributeSchemaImpl& other) const
{
    IE_CONFIG_ASSERT_EQUAL(mAttrName2IdMap, other.mAttrName2IdMap, 
                           "mAttrName2IdMap not equal");
    IE_CONFIG_ASSERT_EQUAL(mAttrConfigs.size(), other.mAttrConfigs.size(), 
                           "mAttrConfigs's size not equal");

    for (size_t i = 0; i < mAttrConfigs.size(); ++i)
    {
        mAttrConfigs[i]->AssertEqual(*(other.mAttrConfigs[i]));
    }

    IE_CONFIG_ASSERT_EQUAL(mPackAttrConfigs.size(), other.mPackAttrConfigs.size(), 
                           "mPackAttrConfigs's size not equal");
    
    for (size_t i = 0; i < mPackAttrConfigs.size(); ++i)
    {
        mPackAttrConfigs[i]->AssertEqual(*(other.mPackAttrConfigs[i]));
    }
}

void AttributeSchemaImpl::AssertCompatible(const AttributeSchemaImpl& other) const
{
    IE_CONFIG_ASSERT(mAttrName2IdMap.size() <=  other.mAttrName2IdMap.size(),
                           "mAttrName2IdMap's size not compatible");
    IE_CONFIG_ASSERT(mAttrConfigs.size() <= other.mAttrConfigs.size(), 
                           "mAttrConfigs' size not compatible");
    size_t i;
    for (i = 0; i < mAttrConfigs.size(); ++i)
    {
        mAttrConfigs[i]->AssertEqual(*(other.mAttrConfigs[i]));
    }
}

void AttributeSchemaImpl::Jsonize(autil::legacy::Jsonizable::JsonWrapper& json) 
{
    if (json.GetMode() == TO_JSON)
    {
        vector<Any> values;
        int32_t attrCount = (mBaseAttrCount >= 0) ?
                            mBaseAttrCount : (int32_t)mAttrConfigs.size();
        for (int32_t i = 0; i < attrCount; i++)
        {
            Any attrField;
            const AttributeConfigPtr &attrConf = mAttrConfigs[i];
            if (!attrConf->GetPackAttributeConfig() &&
                attrConf->GetConfigType() != AttributeConfig::ct_index_accompany)
            {
                attrField = ToJson(attrConf->GetAttrName());
                values.push_back(attrField);
            }
        }

        for (size_t i = 0; i < mPackAttrConfigs.size(); i++)
        {
            values.push_back(ToJson(*mPackAttrConfigs[i]));
        }
        json.Jsonize(ATTRIBUTES, values);
    }
}

attrid_t AttributeSchemaImpl::GetAttributeId(const string& attrName) const
{
    NameMap::const_iterator it = mAttrName2IdMap.find(attrName);
    if (it != mAttrName2IdMap.end())
    {
        return it->second;
    }
    return INVALID_ATTRID;
}

bool AttributeSchemaImpl::HasSameAttributeConfigs(const AttributeSchemaImplPtr& other) const
{
    if (!other || mAttrConfigs.size() != other->mAttrConfigs.size())
    {
        return false;
    }

    auto attrConfigs = mAttrConfigs;
    auto otherAttrConfigs = other->mAttrConfigs;
    static auto cmp = [=](const AttributeConfigPtr& a, const AttributeConfigPtr& b)->bool
    {
        return a->GetAttrName() < b->GetAttrName();
    };
    sort(attrConfigs.begin(), attrConfigs.end(), cmp);
    sort(otherAttrConfigs.begin(), otherAttrConfigs.end(), cmp);
    for (size_t i = 0; i < attrConfigs.size(); i ++)
    {
        if (attrConfigs[i]->GetAttrName() != otherAttrConfigs[i]->GetAttrName())
        {
            return false;
        }
    }
    return true;
}

bool AttributeSchemaImpl::DisableAttribute(const string& attrName)
{
    AttributeConfigPtr attrConfig = GetAttributeConfig(attrName);
    if (attrConfig == NULL_ATTR_CONF)
    {
        return false;
    }
    if (attrConfig->GetPackAttributeConfig())
    {
        IE_LOG(ERROR, "attributs [%s] in pack [%s] does not support disable",
               attrName.c_str(), attrConfig->GetPackAttributeConfig()->GetAttrName().c_str());
        return false;
    }
    attrConfig->Disable();
    return true;
}

bool AttributeSchemaImpl::DisablePackAttribute(const string& packAttrName)
{
    PackAttributeConfigPtr packAttrConfig = GetPackAttributeConfig(packAttrName);
    if (packAttrConfig == NULL_PACK_ATTR_CONF)
    {
        return false;
    }
    packAttrConfig->Disable();
    return true;
}

void AttributeSchemaImpl::DeleteAttribute(const string& attrName)
{
    AttributeConfigPtr attrConfig = GetAttributeConfig(attrName);
    if (attrConfig == NULL_ATTR_CONF)
    {
        INDEXLIB_FATAL_ERROR(Schema, "delete attribute [%s] fail, not exist!", attrName.c_str());
    }

    attrConfig->Delete();
    mAttrName2IdMap.erase(attrName);    
}

void AttributeSchemaImpl::SetBaseSchemaImmutable()
{
    if (mBaseAttrCount < 0)
    {
        mBaseAttrCount = (int32_t)mAttrConfigs.size();
    }
}

void AttributeSchemaImpl::SetModifySchemaImmutable()
{
    if (mBaseAttrCount < 0)
    {
        mBaseAttrCount = (int32_t)mAttrConfigs.size();
    }
    mOnlyAddVirtual = true;
}

void AttributeSchemaImpl::SetModifySchemaMutable()
{
    mOnlyAddVirtual = false;
}

AttributeConfigIteratorPtr AttributeSchemaImpl::CreateIterator(
    ConfigIteratorType type) const
{
    vector<AttributeConfigPtr> ret;
    for (auto &attribute : mAttrConfigs)
    {
        assert(attribute);
        if ((attribute->GetStatus() & type) > 0)
        {
            ret.push_back(attribute);
        }
    }
    AttributeConfigIteratorPtr iter(new AttributeConfigIterator(ret));
    return iter;
}

PackAttributeConfigIteratorPtr AttributeSchemaImpl::CreatePackAttrIterator(
    ConfigIteratorType type) const
{
    assert(type == CIT_NORMAL || type == CIT_DISABLE);
    vector<PackAttributeConfigPtr> ret;
    for (auto &attribute : mPackAttrConfigs)
    {
        assert(attribute);
        if (type == CIT_NORMAL && !attribute->IsDisable())
        {
            ret.push_back(attribute);
        }
        else if (type == CIT_DISABLE && attribute->IsDisable())
        {
            ret.push_back(attribute);
        }
    }
    PackAttributeConfigIteratorPtr iter(new PackAttributeConfigIterator(ret));
    return iter;
}

void AttributeSchemaImpl::CollectBaseVersionAttrInfo(ModifyItemVector& attrs) const
{
    int32_t attrCount = (mBaseAttrCount >= 0) ?
                        mBaseAttrCount : (int32_t)mAttrConfigs.size();
    for (int32_t i = 0; i < attrCount; i++)
    {
        const AttributeConfigPtr &attrConf = mAttrConfigs[i];
        if (attrConf->IsDeleted())
        {
            continue;
        }
        ModifyItemPtr item(new ModifyItem(attrConf->GetAttrName(),
                        FieldConfig::FieldTypeToStr(attrConf->GetFieldType()),
                        INVALID_SCHEMA_OP_ID));
        attrs.push_back(item);
    }
}

IE_NAMESPACE_END(config);

