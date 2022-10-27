#include "indexlib/index/normal/inverted_index/customized_index/index_plugin_util.h"

using namespace std;

IE_NAMESPACE_USE(plugin);
IE_NAMESPACE_USE(config);
IE_NAMESPACE_USE(index_base);

IE_NAMESPACE_BEGIN(index);

IE_LOG_SETUP(index, IndexPluginUtil);

std::string IndexPluginUtil::INDEX_MODULE_FACTORY_SUFFIX = "Factory_Index";

IndexPluginUtil::IndexPluginUtil() 
{
}

IndexPluginUtil::~IndexPluginUtil() 
{
}

IndexModuleFactory* IndexPluginUtil::GetFactory(
        const CustomizedIndexConfigPtr& indexConfig,
        const PluginManagerPtr& pluginManager)
{
    assert(indexConfig);
    const auto& indexerName = indexConfig->GetIndexerName();
    if (!pluginManager)
    {
        IE_LOG(ERROR, "index[%s], indexerName[%s], pluginManager is NULL",
               indexConfig->GetIndexName().c_str(), indexerName.c_str());
        return NULL;
    }    
    Module* module = pluginManager->getModule(indexerName);
    if (!module)
    {
        IE_LOG(ERROR, "get module[%s] failed", indexerName.c_str());
        return NULL;
    }
    IndexModuleFactory *factory =
        dynamic_cast<IndexModuleFactory*>(
            module->getModuleFactory(INDEX_MODULE_FACTORY_SUFFIX));
    if (!factory)
    {
        IE_LOG(ERROR, "get module factory[%s] failed", indexerName.c_str());
        return NULL;
    }
    return factory;
}

Indexer* IndexPluginUtil::CreateIndexer(const IndexConfigPtr& indexConfig,
                                        const PluginManagerPtr& pluginManager)
{
    if (!indexConfig)
    {
        IE_LOG(ERROR, "indexConfig is NULL");
        return NULL;
    }
    auto customizedIndexConfig = DYNAMIC_POINTER_CAST(
            CustomizedIndexConfig, indexConfig);
    if (!customizedIndexConfig)
    {
        IE_LOG(ERROR, "index[%s] is not CustomizedIndexConfig",
               indexConfig->GetIndexName().c_str());
        return NULL;
    }        
    auto factory = GetFactory(customizedIndexConfig, pluginManager);
    if (!factory)
    {
        return NULL;
    }
    auto indexer = factory->createIndexer(customizedIndexConfig->GetParameters());
    if (!indexer)
    {
        IE_LOG(ERROR, "create indexer for index [%s] failed",
               indexConfig->GetIndexName().c_str()); 
    }
    return indexer;
}

IndexReducer* IndexPluginUtil::CreateIndexReducer(
        const IndexConfigPtr& indexConfig,
        const PluginManagerPtr& pluginManager)
{
    if (!indexConfig)
    {
        IE_LOG(ERROR, "indexConfig is NULL");
        return NULL;
    }
    auto customizedIndexConfig = DYNAMIC_POINTER_CAST(
            CustomizedIndexConfig, indexConfig);
    if (!customizedIndexConfig)
    {
        IE_LOG(ERROR, "index[%s] is not CustomizedIndexConfig",
               indexConfig->GetIndexName().c_str());
        return NULL;
    }        
    auto factory = GetFactory(customizedIndexConfig, pluginManager);    
    if (!factory)
    {
        return NULL;
    }
    
    IndexReducer* indexReducer = factory->createIndexReducer(
            customizedIndexConfig->GetParameters());
    if (!indexReducer)
    {
        IE_LOG(ERROR, "create indexReducer for index [%s] failed",
               indexConfig->GetIndexName().c_str());
        return NULL;
    }
    return indexReducer;
}

IndexSegmentRetriever* IndexPluginUtil::CreateIndexSegmentRetriever(
        const IndexConfigPtr& indexConfig,
        const PluginManagerPtr& pluginManager)
{
    if (!indexConfig)
    {
        IE_LOG(ERROR, "indexConfig is NULL");
        return NULL;
    }
    auto customizedIndexConfig = DYNAMIC_POINTER_CAST(
            CustomizedIndexConfig, indexConfig);
    if (!customizedIndexConfig)
    {
        IE_LOG(ERROR, "index[%s] is not CustomizedIndexConfig",
               indexConfig->GetIndexName().c_str());
        return NULL;
    }        
    
    auto factory = GetFactory(customizedIndexConfig, pluginManager);    
    if (!factory)
    {
        return NULL;
    }
    IndexSegmentRetriever* indexSegRetriever = factory->createIndexSegmentRetriever(
            customizedIndexConfig->GetParameters());
    if (!indexSegRetriever)
    {
        IE_LOG(ERROR, "create indexSegRetriever for index [%s] failed",
               indexConfig->GetIndexName().c_str());
        return NULL;
    }
    return indexSegRetriever;
}

IndexRetriever* IndexPluginUtil::CreateIndexRetriever(
        const IndexConfigPtr& indexConfig,
        const PluginManagerPtr& pluginManager)
{
    if (!indexConfig)
    {
        IE_LOG(ERROR, "indexConfig is NULL");
        return NULL;
    }
    auto customizedIndexConfig = DYNAMIC_POINTER_CAST(
            CustomizedIndexConfig, indexConfig);
    if (!customizedIndexConfig)
    {
        IE_LOG(ERROR, "index[%s] is not CustomizedIndexConfig",
               indexConfig->GetIndexName().c_str());
        return NULL;
    }        
    auto factory = GetFactory(customizedIndexConfig, pluginManager);    
    if (!factory)
    {
        return NULL;
    }
    IndexRetriever* indexRetriever = factory->createIndexRetriever(
            customizedIndexConfig->GetParameters());
    if (!indexRetriever)
    {
        IE_LOG(ERROR, "create IndexRetriever for index [%s] failed",
               indexConfig->GetIndexName().c_str());
        return NULL;
    }
    return indexRetriever;
}

IndexerUserData* IndexPluginUtil::CreateIndexerUserData(
        const IndexConfigPtr& indexConfig,
        const PluginManagerPtr& pluginManager)
{
    if (!indexConfig)
    {
        IE_LOG(ERROR, "indexConfig is NULL");
        return NULL;
    }
    auto customizedIndexConfig = DYNAMIC_POINTER_CAST(
            CustomizedIndexConfig, indexConfig);
    if (!customizedIndexConfig)
    {
        IE_LOG(ERROR, "index[%s] is not CustomizedIndexConfig",
               indexConfig->GetIndexName().c_str());
        return NULL;
    }        
    auto factory = GetFactory(customizedIndexConfig, pluginManager);
    if (!factory)
    {
        return NULL;
    }
    
    auto userData = factory->createIndexerUserData(customizedIndexConfig->GetParameters());
    if (!userData)
    {
        IE_LOG(INFO, "create IndexerUserData return NULL!");
    }
    return userData;
}

IE_NAMESPACE_END(index);

