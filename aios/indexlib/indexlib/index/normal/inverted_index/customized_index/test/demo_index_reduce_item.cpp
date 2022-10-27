#include "indexlib/index/normal/inverted_index/customized_index/test/demo_index_reduce_item.h"
#include "indexlib/index/normal/inverted_index/customized_index/test/demo_indexer.h"
#include "indexlib/misc/exception.h"
#include <autil/legacy/jsonizable.h>

using namespace std;

IE_NAMESPACE_USE(file_system);
IE_NAMESPACE_USE(misc);

IE_NAMESPACE_BEGIN(index);
IE_LOG_SETUP(index, DemoIndexReduceItem);

const std::string DemoIndexReduceItem::mFileName = "demo_index_file";

DemoIndexReduceItem::DemoIndexReduceItem() 
{
}

DemoIndexReduceItem::~DemoIndexReduceItem() 
{
}

bool DemoIndexReduceItem::LoadIndex(const file_system::DirectoryPtr& indexDir)
{
    string fileContent;
    try
    {
        indexDir->Load(mFileName, fileContent);
        autil::legacy::FromJsonString(mDataMap, fileContent);
    }
    catch(const autil::legacy::ExceptionBase& e)
    {
        IE_LOG(ERROR, "fail to load index file [%s], error: [%s]",
               mFileName.c_str(), e.what());
        return false;
    }
    return true;
}

bool DemoIndexReduceItem::UpdateDocId(const DocIdMap& docIdMap)
{
    std::map<docid_t, string> newDataMap;
    for (const auto& kv : mDataMap)
    {
        docid_t newDocId = docIdMap.GetNewDocId(kv.first);
        if (newDocId == INVALID_DOCID)
        {
            continue;
        }
        newDataMap[newDocId] = kv.second;
    }
    mDataMap.swap(newDataMap);
    return true;
}

bool DemoIndexReduceItem::Add(docid_t docId, const std::string& data)
{
    return mDataMap.insert(make_pair(docId, data)).second;    
}


IE_NAMESPACE_END(index);

