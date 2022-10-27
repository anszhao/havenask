#include "indexlib/index/normal/deletionmap/test/deletion_map_merge_unittest.h"
#include "indexlib/index/normal/deletionmap/deletion_map_merger.h"
#include "indexlib/index/normal/deletionmap/deletion_map_reader.h"
#include "indexlib/index/normal/deletionmap/deletion_map_segment_writer.h"
#include "indexlib/test/partition_data_maker.h"
#include "indexlib/storage/file_system_wrapper.h"
#include "indexlib/merger/segment_directory.h"
#include <autil/StringTokenizer.h>
#include <autil/StringUtil.h>

using namespace std;
using namespace autil;
using namespace fslib;
using namespace fslib::fs;

IE_NAMESPACE_USE(storage);
IE_NAMESPACE_USE(util);
IE_NAMESPACE_USE(partition);
IE_NAMESPACE_USE(index_base);
IE_NAMESPACE_USE(file_system);
IE_NAMESPACE_USE(test);
IE_NAMESPACE_USE(merger);

IE_NAMESPACE_BEGIN(index);
IE_LOG_SETUP(index, DeletionMapMergeTest);

DeletionMapMergeTest::DeletionMapMergeTest()
{
}

DeletionMapMergeTest::~DeletionMapMergeTest()
{
}

void DeletionMapMergeTest::CaseSetUp()
{
    mDir = GET_TEST_DATA_PATH();
}

void DeletionMapMergeTest::CaseTearDown()
{
}

    //merge all: 0,1,2,3,4,5
void DeletionMapMergeTest::TestCaseForMergeAll()
{
    string delStr = "0;0,1;0,2;1,3;2,3,4;0,2,4,5";
    merger::SegmentDirectoryPtr segDir = MakeFakeDeletionMapData(delStr);
    string mergePlan = "0,1,2,3,4,5";
    SegmentMergeInfos segMergeInfos;
    CreateSegmentMergeInfos(mergePlan, segMergeInfos);
        
    DeletionMapMerger merger;
    merger.BeginMerge(segDir);
        
    string mergedDir = mDir + "merged_dir/";
    MergerResource resource;
    resource.targetSegmentCount = 1;
    resource.reclaimMap = ReclaimMapPtr();
    OutputSegmentMergeInfo segInfo;
    segInfo.targetSegmentIndex = 0;
    segInfo.path = mergedDir;
    segInfo.directory = DirectoryCreator::Create(segInfo.path);
    merger.Merge(resource, segMergeInfos, { segInfo });

    for (size_t i = 0; i < segDir->GetVersion().GetSegmentCount(); i++)
    {
        stringstream ss;
        ss << mergedDir << DELETION_MAP_FILE_NAME_PREFIX 
           << "_" << i;
        INDEXLIB_TEST_TRUE(!FileSystemWrapper::IsExist(ss.str()));
    }
}

    //merge 1,2,5; 0 does not belong current version
void DeletionMapMergeTest::TestCaseForMergeWithMultiVersion()
{
    string delStr = "0;0,1;0,1,2;0,1,2,3;0,1,2,3,4;5";
    MakeFakeDeletionMapData(delStr);
    Version version(1);
    for (segmentid_t i = 1; i < 6; ++i)
    {
        version.AddSegment(i);
    }
    version.Store(mDir, false);
    merger::SegmentDirectoryPtr segDir(new merger::SegmentDirectory(mDir, version));
    segDir->Init(false, false);

    string mergePlan = "4,5";
    SegmentMergeInfos segMergeInfos;
    CreateSegmentMergeInfos(mergePlan, segMergeInfos);
        
    DeletionMapMerger merger;
    merger.BeginMerge(segDir);
        
    string mergedDir = mDir + "merged_dir/";

    MergerResource resource;
    resource.targetSegmentCount = 1;
    resource.reclaimMap = ReclaimMapPtr();
    OutputSegmentMergeInfo segInfo;
    segInfo.targetSegmentIndex = 0;
    segInfo.path = mergedDir;
    segInfo.directory = DirectoryCreator::Create(segInfo.path);
    merger.Merge(resource, segMergeInfos, { segInfo });

    string tmp = mergedDir + DELETION_MAP_FILE_NAME_PREFIX + "_";
    INDEXLIB_TEST_TRUE(FileSystemWrapper::IsExist(tmp + "1"));
    INDEXLIB_TEST_TRUE(FileSystemWrapper::IsExist(tmp + "2"));
    INDEXLIB_TEST_TRUE(FileSystemWrapper::IsExist(tmp + "3"));

    INDEXLIB_TEST_TRUE(!FileSystemWrapper::IsExist(tmp + "0"));
    INDEXLIB_TEST_TRUE(!FileSystemWrapper::IsExist(tmp + "4"));
    INDEXLIB_TEST_TRUE(!FileSystemWrapper::IsExist(tmp + "5"));
}

    //merge 1,2,4
void DeletionMapMergeTest::TestCaseForMergePartial()
{
    string delStr = "0;0,1;0,2;1,3;2,3,4;0,2,4,5";
    merger::SegmentDirectoryPtr segDir = MakeFakeDeletionMapData(delStr);
        
    string mergePlan = "1,2,4";
    SegmentMergeInfos segMergeInfos;
    CreateSegmentMergeInfos(mergePlan, segMergeInfos);
        
    DeletionMapMerger merger;
    merger.BeginMerge(segDir);
        
    string mergedDir = mDir + "merged_dir/";
    MergerResource resource;
    resource.targetSegmentCount = 1;
    resource.reclaimMap = ReclaimMapPtr();
    OutputSegmentMergeInfo segInfo;
    segInfo.targetSegmentIndex = 0;
    segInfo.path = mergedDir;
    segInfo.directory = DirectoryCreator::Create(segInfo.path);
    merger.Merge(resource, segMergeInfos, { segInfo });

    string tmp = mergedDir + DELETION_MAP_FILE_NAME_PREFIX + "_";
    INDEXLIB_TEST_TRUE(FileSystemWrapper::IsExist(tmp + "3"));

    INDEXLIB_TEST_TRUE(!FileSystemWrapper::IsExist(tmp + "0"));
    INDEXLIB_TEST_TRUE(!FileSystemWrapper::IsExist(tmp + "1"));
    INDEXLIB_TEST_TRUE(!FileSystemWrapper::IsExist(tmp + "2"));
    INDEXLIB_TEST_TRUE(!FileSystemWrapper::IsExist(tmp + "4"));
    INDEXLIB_TEST_TRUE(!FileSystemWrapper::IsExist(tmp + "5"));
}

merger::SegmentDirectoryPtr DeletionMapMergeTest::MakeFakeDeletionMapData(const string& delStr)
{
    StringTokenizer st(delStr, ";", StringTokenizer::TOKEN_IGNORE_EMPTY
                       | StringTokenizer::TOKEN_TRIM);

    Version version(0);

    for (size_t i = 0; i < st.getNumTokens(); ++i)
    {
        version.AddSegment(i);
        stringstream ss;
        ss << mDir << version.GetSegmentDirName(i) << "/";

        string segmentPath = ss.str();
        string deletionMapPath = segmentPath + DELETION_MAP_DIR_NAME + "/";

        FileSystemWrapper::MkDir(ss.str(), true);
            
        StringTokenizer st2(st[i], ",", StringTokenizer::TOKEN_IGNORE_EMPTY
                            | StringTokenizer::TOKEN_TRIM);

        for (size_t j = 0; j < st2.getNumTokens(); ++j)
        {
            string delDataPath = deletionMapPath + DELETION_MAP_FILE_NAME_PREFIX
                                 + "_" + st2[j];
            File* file = FileSystem::openFile(delDataPath, WRITE);
            file->close();
            delete file;
        }

        SegmentInfo segInfo;
        segInfo.Store(segmentPath + "segment_info");
    }
    
    version.Store(mDir, false);
    merger::SegmentDirectoryPtr segDir(new merger::SegmentDirectory(mDir, version));
    segDir->Init(false, false);
    return segDir;
}

void DeletionMapMergeTest::CreateSegmentMergeInfos(const string& mergePlan, 
        SegmentMergeInfos& segMergeInfos)
{
    StringTokenizer st(mergePlan, ",", StringTokenizer::TOKEN_IGNORE_EMPTY
                       | StringTokenizer::TOKEN_TRIM);

    docid_t baseDocId = 0;
    for (size_t i = 0; i< st.getNumTokens(); ++i)
    {
        segmentid_t segId;
        StringUtil::strToInt32(st[i].c_str(), segId);
        SegmentMergeInfo segMergeInfo;
        segMergeInfo.segmentId = segId;
        segMergeInfo.baseDocId = baseDocId;
        segMergeInfo.segmentInfo.docCount = 100;
        baseDocId += segMergeInfo.segmentInfo.docCount;
        segMergeInfos.push_back(segMergeInfo);
    }
}

merger::SegmentDirectoryPtr DeletionMapMergeTest::CreateDeletionMapData(
        const vector<string> &delSegments)
{
    DirectoryPtr rootDirectory = GET_PARTITION_DIRECTORY();
    Version version(0);
    for (size_t i = 0; i < delSegments.size(); i++)
    {
        segmentid_t currentSegId = (segmentid_t)i;
        version.AddSegment(currentSegId);

        DirectoryPtr segDirectory = 
            rootDirectory->MakeDirectory(version.GetSegmentDirName(currentSegId));
        DirectoryPtr deletionmapDirectory = 
            segDirectory->MakeDirectory(DELETION_MAP_DIR_NAME);

        const string &oneSegment = delSegments[i];
        vector<vector<string> > delDocs;
        StringUtil::fromString(oneSegment, delDocs, ":", ";");
        for (size_t j = 0; j < delDocs.size(); j++)
        {
            assert(delDocs[j].size() == 2);
            DeletionMapSegmentWriter writer;
            writer.Init(100);
            vector<docid_t> delDocIds;
            StringUtil::fromString(delDocs[j][1], delDocIds, ",");
            for (size_t k = 0; k < delDocIds.size(); k++)
            {
                writer.Delete(delDocIds[k]);
            }
            writer.EndSegment(100);
            
            segmentid_t targetSegId;
            StringUtil::fromString(delDocs[j][0], targetSegId);
            writer.Dump(deletionmapDirectory, targetSegId);
        } 
    }
    return merger::SegmentDirectoryPtr(new merger::SegmentDirectory(mDir, version));
}

IE_NAMESPACE_END(index);

