#include "build_service/test/unittest.h"
#include "build_service/builder/AsyncBuilder.h"
#include "build_service/builder/test/MockIndexBuilder.h"
#include "build_service/builder/test/IndexBuilderCreator.h"
#include "build_service/document/test/DocumentTestHelper.h"
#include "FakeThread.h"

using namespace std;
using namespace autil;
using namespace testing;
using namespace build_service::document;
using namespace build_service::config;
IE_NAMESPACE_USE(document);
IE_NAMESPACE_USE(index_base);
IE_NAMESPACE_USE(index);

namespace build_service {
namespace builder {

class AsyncBuilderTest : public BUILD_SERVICE_TESTBASE {
public:
    void setUp();
    void tearDown();
};

void AsyncBuilderTest::setUp() {
}

void AsyncBuilderTest::tearDown() {
}

TEST_F(AsyncBuilderTest, testSimple) {
    MockIndexBuilderPtr mockIndexBuilder =
        IndexBuilderCreator::CreateMockIndexBuilder(GET_TEST_DATA_PATH());
    mockIndexBuilder->Init();
    AsyncBuilder builder(mockIndexBuilder);
    BuilderConfig config;
    config.asyncQueueSize = 5;
    config.asyncBuild = true;
    builder.init(config);
    EXPECT_CALL(*mockIndexBuilder, Build(_))
        .WillOnce(Return(true));
    builder.build(DocumentTestHelper::createDocument(FakeDocument("1")));
    builder.stop();
}

TEST_F(AsyncBuilderTest, testBuild) {
    MockIndexBuilderPtr mockIndexBuilder =
        IndexBuilderCreator::CreateMockIndexBuilder(GET_TEST_DATA_PATH());
    mockIndexBuilder->Init();
    AsyncBuilder builder(mockIndexBuilder);
    BuilderConfig config;
    config.asyncQueueSize = 5;
    config.asyncBuild = true;
    builder.init(config);

    builder._releaseDocQueue->push(
            DocumentTestHelper::createDocument(FakeDocument("1")));
    builder._releaseDocQueue->push(
            DocumentTestHelper::createDocument(FakeDocument("2")));

    EXPECT_CALL(*mockIndexBuilder, Build(_))
        .WillOnce(Return(true));

    ASSERT_TRUE(builder.build(DocumentTestHelper::createDocument(FakeDocument("3"))));
    usleep(50000);
    ASSERT_EQ((uint32_t)1, builder._releaseDocQueue->size());

    //test has fatal error
    builder.setFatalError();
    ASSERT_FALSE(builder.build(DocumentTestHelper::createDocument(FakeDocument("3"))));

    //test stop
    builder.stop();
    ASSERT_FALSE(builder.build(DocumentTestHelper::createDocument(FakeDocument("3"))));
}

TEST_F(AsyncBuilderTest, testBuildThreadNormal) {
    MockIndexBuilderPtr mockIndexBuilder =
        IndexBuilderCreator::CreateMockIndexBuilder(GET_TEST_DATA_PATH());
    mockIndexBuilder->Init();
    AsyncBuilder builder(mockIndexBuilder);
    BuilderConfig config;
    config.asyncQueueSize = 5;
    config.asyncBuild = true;
    builder.init(config);

    EXPECT_CALL(*mockIndexBuilder, Build(_))
        .WillOnce(Return(true))
        .WillOnce(Return(false));

    DocumentPtr doc = DocumentTestHelper::createDocument(FakeDocument("1"));
    builder._docQueue->push(doc, 0);
    usleep(50000);
    ASSERT_EQ((uint32_t)0, builder._docQueue->size());
    ASSERT_EQ((uint32_t)1, builder._releaseDocQueue->size());
    ASSERT_TRUE(builder._running);

    DocumentPtr doc1 = DocumentTestHelper::createDocument(FakeDocument("1"));
    builder._docQueue->push(doc1, 0);
    usleep(50000);
    ASSERT_EQ((uint32_t)0, builder._docQueue->size());
    ASSERT_EQ((uint32_t)2, builder._releaseDocQueue->size());
    ASSERT_TRUE(builder._running);
}

TEST_F(AsyncBuilderTest, testStop) {
    MockIndexBuilderPtr mockIndexBuilder =
        IndexBuilderCreator::CreateMockIndexBuilder(GET_TEST_DATA_PATH());
    mockIndexBuilder->Init();
    AsyncBuilder builder(mockIndexBuilder);
    BuilderConfig config;
    config.asyncQueueSize = 5;
    config.asyncBuild = true;
    builder.init(config);

    EXPECT_CALL(*mockIndexBuilder, Build(_))
        .WillOnce(Return(true))
        .WillOnce(Return(true))
        .WillOnce(Return(true));
    ASSERT_TRUE(builder.build(DocumentTestHelper::createDocument(FakeDocument("1"))));
    ASSERT_TRUE(builder.build(DocumentTestHelper::createDocument(FakeDocument("2"))));
    ASSERT_TRUE(builder.build(DocumentTestHelper::createDocument(FakeDocument("3"))));

    builder.stop();
    ASSERT_FALSE(builder._running);
    ASSERT_TRUE(builder._asyncBuildThreadPtr == NULL);
    ASSERT_EQ((uint32_t)0, builder._releaseDocQueue->size());
}

TEST_F(AsyncBuilderTest, testInitFail) {
    MockIndexBuilderPtr mockIndexBuilder =
        IndexBuilderCreator::CreateMockIndexBuilder(GET_TEST_DATA_PATH());
    mockIndexBuilder->Init();
    BuilderConfig config;
    config.asyncQueueSize = 5;
    config.asyncBuild = true;

    setCreateThreadFail(true);
    AsyncBuilder builder(mockIndexBuilder);
    ASSERT_FALSE(builder.init(config));
    ASSERT_FALSE(builder._running);
    setCreateThreadFail(false);
}

TEST_F(AsyncBuilderTest, testInit) {
    MockIndexBuilderPtr mockIndexBuilder =
        IndexBuilderCreator::CreateMockIndexBuilder(GET_TEST_DATA_PATH());
    mockIndexBuilder->Init();
    BuilderConfig config;
    config.asyncQueueSize = 5;
    config.asyncBuild = true;

    //test init normal
    AsyncBuilder builder(mockIndexBuilder);
    ASSERT_TRUE(builder.init(config));
    ASSERT_TRUE(builder._running);
    ASSERT_TRUE(builder._asyncBuildThreadPtr != NULL);
    ASSERT_EQ((uint32_t)5, builder._docQueue->capacity());
    ASSERT_EQ((uint32_t)7, builder._releaseDocQueue->capacity());
}
}
}
