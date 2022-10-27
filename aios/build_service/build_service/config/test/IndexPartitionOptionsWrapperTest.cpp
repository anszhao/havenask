#include "build_service/test/unittest.h"
#include "build_service/config/IndexPartitionOptionsWrapper.h"
#include "build_service/config/ResourceReaderManager.h"

using namespace std;
using namespace testing;

namespace build_service {
namespace config {

class IndexPartitionOptionsWrapperTest : public BUILD_SERVICE_TESTBASE {};

TEST_F(IndexPartitionOptionsWrapperTest, testSimple) {
    ResourceReaderPtr resourceReader =
        ResourceReaderManager::getResourceReader(
            TEST_DATA_PATH"//index_partition_option_wrapper_test/normal/");
    IndexPartitionOptionsWrapper wrapper;
    ASSERT_TRUE(wrapper.load(*(resourceReader.get()), "simple"));
    {
        IE_NAMESPACE(config)::IndexPartitionOptions indexOptions;
        EXPECT_TRUE(wrapper.getIndexPartitionOptions("inc", indexOptions));
        ASSERT_TRUE(indexOptions.GetMergeConfig().truncateOptionConfig);
        EXPECT_EQ(uint32_t(5), indexOptions.GetBuildConfig(true).keepVersionCount);
        EXPECT_EQ(uint32_t(4), indexOptions.GetBuildConfig(false).keepVersionCount);
    }
    {
        IE_NAMESPACE(config)::IndexPartitionOptions indexOptions;
        EXPECT_TRUE(wrapper.getIndexPartitionOptions("full", indexOptions));
        EXPECT_TRUE(wrapper.hasIndexPartitionOptions("full"));
        EXPECT_EQ(uint32_t(4), indexOptions.GetBuildConfig(false).keepVersionCount);
    }
    {
        IE_NAMESPACE(config)::IndexPartitionOptions indexOptions;
        EXPECT_TRUE(wrapper.getIndexPartitionOptions("", indexOptions));
        EXPECT_TRUE(wrapper.hasIndexPartitionOptions(""));
        EXPECT_EQ(uint32_t(4), indexOptions.GetBuildConfig(false).keepVersionCount);
        EXPECT_EQ("abc", indexOptions.GetMergeConfig().mergeStrategyParameter.strategyConditions);
    }
    {
        IE_NAMESPACE(config)::IndexPartitionOptions indexOptions;
        EXPECT_TRUE(IndexPartitionOptionsWrapper::getIndexPartitionOptions(
                        *(resourceReader.get()), "simple", "", indexOptions));
        EXPECT_EQ(uint32_t(4), indexOptions.GetBuildConfig(false).keepVersionCount);
        EXPECT_EQ("abc", indexOptions.GetMergeConfig().mergeStrategyParameter.strategyConditions);
    }
    {
        IE_NAMESPACE(config)::IndexPartitionOptions indexOptions;
        EXPECT_FALSE(wrapper.getIndexPartitionOptions("not exist", indexOptions));
        EXPECT_FALSE(wrapper.hasIndexPartitionOptions("not exist"));
    }
}

}
}
