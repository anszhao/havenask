#include "build_service/test/unittest.h"
#include "build_service/config/SwiftConfig.h"
#include "build_service/proto/BasicDefs.pb.h"
#include "build_service/config/CLIOptionNames.h"
#include <regex>

using namespace std;
using namespace testing;
using namespace build_service::proto;

namespace build_service {
namespace config {

class SwiftConfigTest : public BUILD_SERVICE_TESTBASE {
public:
    void setUp();
    void tearDown();
};

void SwiftConfigTest::setUp() {
}

void SwiftConfigTest::tearDown() {
}

TEST_F(SwiftConfigTest, testSimple) {
    string configStr = R"(
{
    "reader_config" : "test_reader_config",
    "writer_config" : "test_writer_config",
    "swift_client_share_mode" : "share",
    "swift_client_config": "maxBufferSize=3", 
    "topic_full" :
    {
        "partition_count" : 10,
        "partition_limit" : 2,
        "partition_resource" : 50,
        "delete_topic_data" : true,
        "writer_max_buffer_size" : {
            "simple.file" : 100
        }
    },
    "topic_inc" :
    {
        "partition_count" : 2,
        "partition_limit" : 2,
        "partition_resource" : 1,
        "delete_topic_data" : false
    }
})";
    SwiftConfig swiftConfig;
    {
        EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
        EXPECT_TRUE(swiftConfig.validate());
        EXPECT_FALSE(swiftConfig.isFullIncShareBrokerTopic());
        EXPECT_EQ(string("test_reader_config"), swiftConfig.getSwiftReaderConfig(proto::BUILD_STEP_FULL));
        EXPECT_EQ(string("test_writer_config"), swiftConfig.getSwiftWriterConfig(proto::BUILD_STEP_FULL));
        EXPECT_EQ(string("test_reader_config"),
                  swiftConfig.getSwiftReaderConfig(BUILD_STEP_INC));
        EXPECT_EQ(string("test_writer_config"),
                  swiftConfig.getSwiftWriterConfig(BUILD_STEP_INC));
        EXPECT_EQ(string("maxBufferSize=3"),
                  swiftConfig.getSwiftClientConfig(BUILD_STEP_INC));
        EXPECT_EQ(50u, swiftConfig.getFullBrokerTopicConfig()->get().resource());
        EXPECT_TRUE(swiftConfig.getFullBrokerTopicConfig()->get().deletetopicdata());
    
        EXPECT_EQ(1u, swiftConfig.getIncBrokerTopicConfig()->get().resource()); 
        EXPECT_FALSE(swiftConfig.getIncBrokerTopicConfig()->get().deletetopicdata());

        EXPECT_EQ(100u, swiftConfig.getFullBrokerTopicConfig()->writerMaxBufferSize["simple.file"]);
        EXPECT_TRUE(swiftConfig.getIncBrokerTopicConfig()->writerMaxBufferSize.empty());        
    }
    SwiftConfig newSwiftConfig = swiftConfig;
    {
        EXPECT_TRUE(newSwiftConfig.validate());
        EXPECT_FALSE(newSwiftConfig.isFullIncShareBrokerTopic());
        EXPECT_EQ(string("test_reader_config"), newSwiftConfig.getSwiftReaderConfig(proto::BUILD_STEP_FULL));
        EXPECT_EQ(string("test_writer_config"), newSwiftConfig.getSwiftWriterConfig(proto::BUILD_STEP_FULL));
        EXPECT_EQ(string("test_reader_config"),
                  newSwiftConfig.getSwiftReaderConfig(BUILD_STEP_INC));
        EXPECT_EQ(string("test_writer_config"),
                  newSwiftConfig.getSwiftWriterConfig(BUILD_STEP_INC));
        EXPECT_EQ(string("maxBufferSize=3"),
                  newSwiftConfig.getSwiftClientConfig(BUILD_STEP_INC));
        EXPECT_EQ(50u, newSwiftConfig.getFullBrokerTopicConfig()->get().resource());
        EXPECT_TRUE(newSwiftConfig.getFullBrokerTopicConfig()->get().deletetopicdata());
    
        EXPECT_EQ(1u, newSwiftConfig.getIncBrokerTopicConfig()->get().resource()); 
        EXPECT_FALSE(newSwiftConfig.getIncBrokerTopicConfig()->get().deletetopicdata());

        EXPECT_EQ(100u, newSwiftConfig.getFullBrokerTopicConfig()->writerMaxBufferSize["simple.file"]);
        EXPECT_TRUE(newSwiftConfig.getIncBrokerTopicConfig()->writerMaxBufferSize.empty());                
    }
}

TEST_F(SwiftConfigTest, testGetConfigFromTopicConfig) {
    string configStr = R"(
{
    "reader_config" : "default_reader_config",
    "writer_config" : "default_writer_config",
    "swift_client_config" : "default_client_config",
    "topic_full" :
    {
        "reader_config" : "full_reader_config",
        "writer_config" : "full_writer_config",
        "swift_client_config" : "full_client_config"
    },
    "topic_inc" :
    {
        "reader_config" : "inc_reader_config",
        "writer_config" : "inc_writer_config",
        "swift_client_config" : "inc_client_config"
    }
})";
    SwiftConfig swiftConfig;
    EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
    EXPECT_TRUE(swiftConfig.validate());
    EXPECT_EQ(string("full_reader_config"),
              swiftConfig.getSwiftReaderConfig(proto::BUILD_STEP_FULL));
    EXPECT_EQ(string("full_writer_config"),
              swiftConfig.getSwiftWriterConfig(proto::BUILD_STEP_FULL));
    EXPECT_EQ(string("full_client_config"),
              swiftConfig.getSwiftClientConfig(proto::BUILD_STEP_FULL));
    EXPECT_EQ(string("inc_reader_config"),
              swiftConfig.getSwiftReaderConfig(BUILD_STEP_INC));
    EXPECT_EQ(string("inc_writer_config"),
              swiftConfig.getSwiftWriterConfig(BUILD_STEP_INC));
    EXPECT_EQ(string("inc_client_config"),
              swiftConfig.getSwiftClientConfig(BUILD_STEP_INC));
}

TEST_F(SwiftConfigTest, testValidate) {
    {   // test specify nothing
        string configStr = R"(
        {
        })";
        SwiftConfig swiftConfig;
        EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
        EXPECT_TRUE(swiftConfig.validate());
        EXPECT_TRUE(swiftConfig.isFullIncShareBrokerTopic());
        EXPECT_EQ(string(""), swiftConfig.getSwiftReaderConfig(proto::BUILD_STEP_FULL));
        EXPECT_EQ(SwiftConfig::DEFAULT_SWIFT_WRITER_CONFIG, swiftConfig.getSwiftWriterConfig(proto::BUILD_STEP_FULL));
    }
    {   // test specify topic only
        string configStr = R"(
        {
            "topic" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "delete_topic_data" : true
            }
        })";
        SwiftConfig swiftConfig;
        EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
        EXPECT_TRUE(swiftConfig.validate());
        EXPECT_TRUE(swiftConfig.isFullIncShareBrokerTopic());
        EXPECT_EQ(string(""), swiftConfig.getSwiftReaderConfig(proto::BUILD_STEP_FULL));
        EXPECT_EQ(SwiftConfig::DEFAULT_SWIFT_WRITER_CONFIG, swiftConfig.getSwiftWriterConfig(proto::BUILD_STEP_FULL));

        EXPECT_EQ(50u, swiftConfig.getFullBrokerTopicConfig()->get().resource());
        EXPECT_TRUE(swiftConfig.getFullBrokerTopicConfig()->get().deletetopicdata()); 
        EXPECT_EQ(50u, swiftConfig.getIncBrokerTopicConfig()->get().resource()); 
        EXPECT_TRUE(swiftConfig.getIncBrokerTopicConfig()->get().deletetopicdata()); 
    }
    {   // test specify topic and topic_full
        string configStr = R"(
        {
            "topic" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "delete_topic_data" : true
            },
            "topic_full" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "delete_topic_data" : true
            }
        })";
        SwiftConfig swiftConfig;
        EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
        EXPECT_FALSE(swiftConfig.validate());
    }
    {   // test specify topic and topic_inc
        string configStr = R"(
        {
            "topic" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "delete_topic_data" : true
            },
            "topic_inc" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "delete_topic_data" : true
            }
        })";
        SwiftConfig swiftConfig;
        EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
        EXPECT_FALSE(swiftConfig.validate());
    }
    {   // test specify topic and topic_inc and topic_full
        string configStr = R"(
        {
            "topic" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "delete_topic_data" : true
            },
            "topic_full" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "delete_topic_data" : true
            },
            "topic_inc" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "delete_topic_data" : true
            }
        })";
        SwiftConfig swiftConfig;
        EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
        EXPECT_FALSE(swiftConfig.validate());
    } 
}

TEST_F(SwiftConfigTest, testMemoryTopicValidate) {
    {   // default topic can not have memory prefer topic
        string configStr = R"(
        {
            "topic" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "topic_mode" : "memory_prefer"
            }
        })";
        SwiftConfig swiftConfig;
        EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
        EXPECT_FALSE(swiftConfig.validate());
    }
    {   // inc topic can not have memory prefer topic
        string configStr = R"(
        {
            "topic_inc" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "topic_mode" : "memory_prefer"
            },
            "topic_full" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "topic_mode" : "memory_prefer"
            }
        })";
        SwiftConfig swiftConfig;
        EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
        EXPECT_FALSE(swiftConfig.validate());
    }
    {   // inc topic can not have memory prefer topic
        string configStr = R"(
        {
            "topic_inc" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "topic_mode" : "normal"
            },
            "topic_full" :
            {
                "partition_count" : 10,
                "partition_limit" : 2,
                "partition_resource" : 50,
                "topic_mode" : "memory_prefer",
                "no_more_msg_period" : 61
            }
        })";
        SwiftConfig swiftConfig;
        EXPECT_NO_THROW(FromJsonString(swiftConfig, configStr));
        EXPECT_TRUE(swiftConfig.validate());

        auto fullTopicConfig = swiftConfig.getFullBrokerTopicConfig();
        EXPECT_EQ(61, fullTopicConfig->noMoreMsgPeriod);
    }
}

TEST_F(SwiftConfigTest, testGetSwiftWriterConfig) {
    {
        SwiftConfig swiftConfig;
        swiftConfig._writerConfigStr = "functionChain=hashId2partId;mode=async|safe;maxBufferSize=201;teststr";
        EXPECT_EQ(string("functionChain=hashId2partId;mode=async|safe;maxBufferSize=100;teststr"),
                  swiftConfig.getSwiftWriterConfig(100,proto::BUILD_STEP_FULL));        
    } 
    {
        SwiftConfig swiftConfig;
        swiftConfig._writerConfigStr = "functionChain=hashId2partId;mode=async|safe;";
        EXPECT_EQ(string("maxBufferSize=100;functionChain=hashId2partId;mode=async|safe;"),
                  swiftConfig.getSwiftWriterConfig(100,proto::BUILD_STEP_FULL));        
    } 
    {
        SwiftConfig swiftConfig;
        swiftConfig._writerConfigStr = "functionChain=hashId2partId;mode=async|safe;maxBufferSize=9";
        EXPECT_EQ(string("functionChain=hashId2partId;mode=async|safe;maxBufferSize=1"),
                  swiftConfig.getSwiftWriterConfig(1,proto::BUILD_STEP_FULL));        
    }
    {
        SwiftConfig swiftConfig;
        swiftConfig._writerConfigStr = "";
        EXPECT_EQ(string("maxBufferSize=1;"),
                  swiftConfig.getSwiftWriterConfig(1, proto::BUILD_STEP_FULL));        
    }     
}

}
}
