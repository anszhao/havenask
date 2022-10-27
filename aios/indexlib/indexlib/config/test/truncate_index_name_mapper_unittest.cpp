#include "indexlib/common_define.h"
#include "indexlib/test/unittest.h"
#include "indexlib/config/truncate_index_name_mapper.h"
#include "indexlib/test/test.h"
#include "indexlib/config/index_partition_schema.h"
#include "indexlib/config/index_partition_schema_maker.h"
#include "indexlib/config/test/schema_loader.h"

using namespace std;
using namespace autil;

IE_NAMESPACE_BEGIN(config);

class TruncateIndexNameMapperTest : public INDEXLIB_TESTBASE
{
public:
    DECLARE_CLASS_NAME(TruncateIndexNameMapperTest);
    void CaseSetUp() override
    {
        mRootDir = GET_TEST_DATA_PATH();
    }

    void CaseTearDown() override
    {
    }

    void TestCaseForLoad()
    {
        TruncateIndexNameMapper truncMapper(
                string(TEST_DATA_PATH) + string("truncate_index_mapper"));
        truncMapper.Load();
        {
            vector<string> truncNames;
            INDEXLIB_TEST_TRUE(truncMapper.Lookup("index1", truncNames));
            INDEXLIB_TEST_EQUAL((size_t)2, truncNames.size());
            INDEXLIB_TEST_EQUAL(string("index1_trunc1"), truncNames[0]);
            INDEXLIB_TEST_EQUAL(string("index1_trunc2"), truncNames[1]);
        }
        
        {
            vector<string> truncNames;
            INDEXLIB_TEST_TRUE(truncMapper.Lookup("index2", truncNames));
            INDEXLIB_TEST_EQUAL((size_t)3, truncNames.size());
            INDEXLIB_TEST_EQUAL(string("index2_trunc1"), truncNames[0]);
            INDEXLIB_TEST_EQUAL(string("index2_trunc2"), truncNames[1]);
            INDEXLIB_TEST_EQUAL(string("index2_trunc3"), truncNames[2]);
        }

        {
            vector<string> truncNames;
            INDEXLIB_TEST_TRUE(truncMapper.Lookup("index3", truncNames));
            INDEXLIB_TEST_EQUAL((size_t)1, truncNames.size());
            INDEXLIB_TEST_EQUAL(string("index3_trunc1"), truncNames[0]);
        }

        {
            vector<string> truncNames;
            INDEXLIB_TEST_TRUE(!truncMapper.Lookup("index_nonexist", truncNames));
        }
    }

    void TestCaseForDump()
    {
        IndexPartitionSchemaPtr indexPartitionSchema(new IndexPartitionSchema);
        IndexPartitionSchemaMaker::MakeSchema(indexPartitionSchema,
                // field schema
                "title:text;ends:uint32;userid:uint32;nid:uint32;url:string",
                // index schema
                "title:text:title;pk:primarykey64:url",
                // attribute schema
                "ends;userid;nid",
                // summary schema
                "");

        IndexSchemaPtr indexSchema = indexPartitionSchema->GetIndexSchema();
        AddTruncateIndexConfig(indexSchema, "title", "title_trunc1");
        AddTruncateIndexConfig(indexSchema, "title", "title_trunc2");

        TruncateIndexNameMapper truncMapper(mRootDir);
        truncMapper.Dump(indexSchema);

        TruncateIndexNameMapper loadTruncMapper(mRootDir);
        loadTruncMapper.Load();

        vector<string> truncNames;
        INDEXLIB_TEST_TRUE(loadTruncMapper.Lookup("title", truncNames));
        INDEXLIB_TEST_EQUAL((size_t)2, truncNames.size());
        INDEXLIB_TEST_EQUAL(string("title_trunc1"), truncNames[0]);
        INDEXLIB_TEST_EQUAL(string("title_trunc2"), truncNames[1]);
    }

    void TestCaseForDumpShardingIndex()
    {
        IndexPartitionSchemaPtr indexPartitionSchema = SchemaLoader::LoadSchema(
            TEST_DATA_PATH, "simple_sharding_example.json");
        IndexSchemaPtr indexSchema = indexPartitionSchema->GetIndexSchema();
        TruncateIndexNameMapper truncMapper(mRootDir);
        truncMapper.Dump(indexSchema);

        TruncateIndexNameMapper loadTruncMapper(mRootDir);
        loadTruncMapper.Load();
        vector<string> truncNames;
        for (size_t i = 0; i < 4; i++)
        {
            INDEXLIB_TEST_TRUE(
                loadTruncMapper.Lookup("phrase_@_" + StringUtil::toString(i), truncNames));
            INDEXLIB_TEST_EQUAL((size_t)1, truncNames.size());
            INDEXLIB_TEST_TRUE(
                loadTruncMapper.Lookup("expack_@_" + StringUtil::toString(i), truncNames));
            INDEXLIB_TEST_EQUAL((size_t)2, truncNames.size());            
        }
        ASSERT_FALSE(loadTruncMapper.Lookup("phrase", truncNames));
        ASSERT_FALSE(loadTruncMapper.Lookup("expack", truncNames));
        ASSERT_FALSE(loadTruncMapper.Lookup("title", truncNames));
        INDEXLIB_TEST_TRUE(loadTruncMapper.Lookup("title_@_0", truncNames));
        INDEXLIB_TEST_EQUAL((size_t)2, truncNames.size());            
    }

    void AddTruncateIndexConfig(const IndexSchemaPtr& indexSchema,
                                const string& indexName,
                                const string& truncIndexName)
    {
        IndexConfigPtr parentIndexConfig = 
            indexSchema->GetIndexConfig(indexName);
        assert(parentIndexConfig);

        IndexConfigPtr indexConfig(parentIndexConfig->Clone());
        indexConfig->SetIndexName(truncIndexName);
        indexConfig->SetVirtual(true);
        indexConfig->SetNonTruncateIndexName(indexName);
        indexSchema->AddIndexConfig(indexConfig);
    }

    
private:
    string mRootDir;
    IE_LOG_DECLARE();
};

IE_LOG_SETUP(index, TruncateIndexNameMapperTest);

INDEXLIB_UNIT_TEST_CASE(TruncateIndexNameMapperTest, TestCaseForLoad);
INDEXLIB_UNIT_TEST_CASE(TruncateIndexNameMapperTest, TestCaseForDump);
INDEXLIB_UNIT_TEST_CASE(TruncateIndexNameMapperTest, TestCaseForDumpShardingIndex);

IE_NAMESPACE_END(config);
