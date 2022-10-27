#include <ha3/sql/ops/test/OpTestBase.h>
#include <ha3/sql/data/TableUtil.h>
#include <ha3/sql/ops/agg/AggKernel.h>
#include <ha3/sql/ops/agg/builtin/MaxLabelAggFunc.h>

using namespace std;
using namespace matchdoc;
using namespace suez::turing;
using namespace navi;

BEGIN_HA3_NAMESPACE(sql);

class MaxLabelFuncTest : public OpTestBase {
public:
    MaxLabelFuncTest();
    ~MaxLabelFuncTest();
public:
    void setUp() override {
    }
    void tearDown() override {
    }
public:
    MatchDocAllocatorPtr _allocator;
    MaxLabelAggFuncCreator _creator;
};

MaxLabelFuncTest::MaxLabelFuncTest() {
}

MaxLabelFuncTest::~MaxLabelFuncTest() {
}

TEST_F(MaxLabelFuncTest, testMaxLabelFuncCreatorLocal) {
    auto vt = ValueTypeHelper<uint32_t>::getValueType();
    {
        auto func = dynamic_cast<MaxLabelAggFunc<uint32_t, uint32_t> *>(
                _creator.createFunction({vt, vt}, {"label", "value"}, {"label", "max"}, true));
        ASSERT_TRUE(nullptr != func);
        DELETE_AND_SET_NULL(func);
    }
    {
        auto func = _creator.createFunction({ValueTypeHelper<autil::MultiChar>::getValueType(), vt},
                {"label", "value"}, {"label", "max"}, true);
        ASSERT_TRUE(nullptr != func);
        DELETE_AND_SET_NULL(func);
    }
    {
        auto func = _creator.createFunction({vt, ValueTypeHelper<autil::MultiChar>::getValueType()},
                {"label", "value"}, {"label", "max"}, true);
        ASSERT_EQ(nullptr, func);
    }
    {
        auto func = _creator.createFunction({vt}, {"label"}, {"label"}, true);
        ASSERT_EQ(nullptr, func);
    }
}

TEST_F(MaxLabelFuncTest, testLocal) {
    TablePtr table;
    auto vt = ValueTypeHelper<uint32_t>::getValueType();
    {
        vector<MatchDoc> docs = std::move(getMatchDocs(_allocator, 3));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(_allocator, docs, "label", {1, 2, 3}));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(_allocator, docs, "value", {3, 2, 4}));
        table = getTable(createTable(_allocator, docs));
    }
    {
        // test normal
        TablePtr outputTable(new Table(_poolPtr));
        auto func = _creator.createFunction({vt, vt},
                {"label", "value"}, {"label", "max"}, true);
        ASSERT_TRUE(nullptr != func);
        ASSERT_TRUE(func->initAccumulatorOutput(outputTable));
        ASSERT_TRUE(func->initCollectInput(table));
        auto acc = static_cast<MaxLabelAccumulator<uint32_t, uint32_t> *>(func->createAccumulator(_poolPtr.get()));
        ASSERT_TRUE(func->collect(table->getRow(0), acc));
        ASSERT_EQ(3, acc->max);
        ASSERT_EQ(1, acc->label);
        ASSERT_TRUE(func->collect(table->getRow(1), acc));
        ASSERT_EQ(3, acc->max);
        ASSERT_EQ(1, acc->label);
        ASSERT_TRUE(func->collect(table->getRow(2), acc));
        ASSERT_EQ(4, acc->max);
        ASSERT_EQ(3, acc->label);
        ASSERT_TRUE(func->outputAccumulator(acc, outputTable->allocateRow()));
        ASSERT_NO_FATAL_FAILURE(checkOutputColumn<uint32_t>(outputTable, "max", {4}));
        ASSERT_NO_FATAL_FAILURE(checkOutputColumn<uint32_t>(outputTable, "label", {3}));
        POOL_DELETE_CLASS(acc);
        DELETE_AND_SET_NULL(func);
    }
    {
        // test invalid input column name
        TablePtr outputTable(new Table(_poolPtr));
        auto func = _creator.createFunction({vt, vt},
                {"a", "b"}, {"label", "max"}, true);
        ASSERT_TRUE(nullptr != func);
        ASSERT_TRUE(func->initAccumulatorOutput(outputTable));
        ASSERT_FALSE(func->initCollectInput(table));
        DELETE_AND_SET_NULL(func);
    }
    {
        // test invalid input column type
        TablePtr outputTable(new Table(_poolPtr));
        auto vt2 = ValueTypeHelper<uint16_t>::getValueType();
        auto func = _creator.createFunction({vt2, vt2},
                {"label", "value"}, {"label", "max"}, true);
        ASSERT_TRUE(nullptr != func);
        ASSERT_TRUE(func->initAccumulatorOutput(outputTable));
        ASSERT_FALSE(func->initCollectInput(table));
        DELETE_AND_SET_NULL(func);
    }
    {
        // duplicate output column name
        TablePtr outputTable(new Table(_poolPtr));
        outputTable->declareColumn<int>("max");
        auto func = _creator.createFunction({vt, vt},
                {"label", "value"}, {"label", "max"}, true);
        ASSERT_TRUE(nullptr != func);
        ASSERT_FALSE(func->initAccumulatorOutput(outputTable));
        DELETE_AND_SET_NULL(func);
    }
}

TEST_F(MaxLabelFuncTest, testLocalNegative) {
    TablePtr table;
    vector<MatchDoc> docs = std::move(getMatchDocs(_allocator, 3));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(_allocator, docs, "label", {1, 2, 3}));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<int32_t>(_allocator, docs, "value", {-3, -2, -4}));
    table = getTable(createTable(_allocator, docs));
    // test normal
    TablePtr outputTable(new Table(_poolPtr));
    auto func = _creator.createFunction(
            {ValueTypeHelper<uint32_t>::getValueType(), ValueTypeHelper<int32_t>::getValueType()},
            {"label", "value"}, {"label", "max"}, true);
    ASSERT_TRUE(nullptr != func);
    ASSERT_TRUE(func->initAccumulatorOutput(outputTable));
    ASSERT_TRUE(func->initCollectInput(table));
    auto acc = static_cast<MaxLabelAccumulator<uint32_t, int32_t> *>(func->createAccumulator(_poolPtr.get()));
    ASSERT_TRUE(func->collect(table->getRow(0), acc));
    ASSERT_EQ(-3, acc->max);
    ASSERT_EQ(1, acc->label);
    ASSERT_TRUE(func->collect(table->getRow(1), acc));
    ASSERT_EQ(-2, acc->max);
    ASSERT_EQ(2, acc->label);
    ASSERT_TRUE(func->collect(table->getRow(2), acc));
    ASSERT_EQ(-2, acc->max);
    ASSERT_EQ(2, acc->label);
    ASSERT_TRUE(func->outputAccumulator(acc, outputTable->allocateRow()));
    ASSERT_NO_FATAL_FAILURE(checkOutputColumn<int32_t>(outputTable, "max", {-2}));
    ASSERT_NO_FATAL_FAILURE(checkOutputColumn<uint32_t>(outputTable, "label", {2}));
    POOL_DELETE_CLASS(acc);
    DELETE_AND_SET_NULL(func);
}

TEST_F(MaxLabelFuncTest, testLocalMulti) {
    TablePtr table;
    vector<MatchDoc> docs = std::move(getMatchDocs(_allocator, 3));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(_allocator, docs, "label", {1, 2, 3}));
    ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(_allocator, docs, "value",
                    {{2, 1}, {2, 3}, {2, 3, -1}}));
    table = getTable(createTable(_allocator, docs));
    // test normal
    TablePtr outputTable(new Table(_poolPtr));
    auto func = _creator.createFunction(
            {ValueTypeHelper<uint32_t>::getValueType(), ValueTypeHelper<autil::MultiInt32>::getValueType()},
            {"label", "value"}, {"label", "max"}, true);
    ASSERT_TRUE(nullptr != func);
    ASSERT_TRUE(func->initAccumulatorOutput(outputTable));
    ASSERT_TRUE(func->initCollectInput(table));
    auto acc = static_cast<MaxLabelAccumulator<uint32_t, autil::MultiInt32> *>(func->createAccumulator(_poolPtr.get()));
    ASSERT_TRUE(func->collect(table->getRow(0), acc));
    ASSERT_NO_FATAL_FAILURE(checkMultiValue<int32_t>({2, 1}, acc->max));
    ASSERT_EQ(1, acc->label);
    ASSERT_TRUE(func->collect(table->getRow(1), acc));
    ASSERT_NO_FATAL_FAILURE(checkMultiValue<int32_t>({2, 3}, acc->max));
    ASSERT_EQ(2, acc->label);
    ASSERT_TRUE(func->collect(table->getRow(2), acc));
    ASSERT_NO_FATAL_FAILURE(checkMultiValue<int32_t>({2, 3, -1}, acc->max));
    ASSERT_EQ(3, acc->label);
    ASSERT_TRUE(func->outputAccumulator(acc, outputTable->allocateRow()));
    ASSERT_NO_FATAL_FAILURE(checkOutputMultiColumn<int32_t>(outputTable, "max", {{2, 3, -1}}));
    ASSERT_NO_FATAL_FAILURE(checkOutputColumn<uint32_t>(outputTable, "label", {3}));
    POOL_DELETE_CLASS(acc);
    DELETE_AND_SET_NULL(func);
}

TEST_F(MaxLabelFuncTest, testMaxLabelFuncCreatorGlobal) {
    auto vt = ValueTypeHelper<uint32_t>::getValueType();
    {
        auto func = dynamic_cast<MaxLabelAggFunc<uint32_t, uint32_t> *>(
                _creator.createFunction({vt, vt}, {"label", "value"}, {"max"}, false));
        ASSERT_TRUE(nullptr != func);
        DELETE_AND_SET_NULL(func);
    }
    {
        auto func = _creator.createFunction({ValueTypeHelper<autil::MultiChar>::getValueType(), vt},
                {"label", "value"}, {"max"}, false);
        ASSERT_TRUE(nullptr != func);
        DELETE_AND_SET_NULL(func);
    }
    {
        auto func = _creator.createFunction({vt, ValueTypeHelper<autil::MultiChar>::getValueType()},
                {"label", "value"}, {"max"}, false);
        ASSERT_EQ(nullptr, func);
    }
    {
        auto func = _creator.createFunction({vt}, {"label"}, {"label", "max"}, false);
        ASSERT_EQ(nullptr, func);
    }
}

TEST_F(MaxLabelFuncTest, testGlobal) {
    TablePtr table;
    auto vt = ValueTypeHelper<uint32_t>::getValueType();
    {
        vector<MatchDoc> docs = std::move(getMatchDocs(_allocator, 3));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(_allocator, docs, "label", {1, 2, 3}));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(_allocator, docs, "value", {3, 2, 4}));
        table = getTable(createTable(_allocator, docs));
    }
    {
        // test normal
        TablePtr outputTable(new Table(_poolPtr));
        auto func = _creator.createFunction({vt, vt},
                {"label", "value"}, {"label"}, false);
        ASSERT_TRUE(nullptr != func);
        ASSERT_TRUE(func->initResultOutput(outputTable));
        ASSERT_TRUE(func->initMergeInput(table));
        auto acc = static_cast<MaxLabelAccumulator<uint32_t, uint32_t> *>(func->createAccumulator(_poolPtr.get()));
        ASSERT_TRUE(func->merge(table->getRow(0), acc));
        ASSERT_EQ(3, acc->max);
        ASSERT_EQ(1, acc->label);
        ASSERT_TRUE(func->merge(table->getRow(1), acc));
        ASSERT_EQ(3, acc->max);
        ASSERT_EQ(1, acc->label);
        ASSERT_TRUE(func->merge(table->getRow(2), acc));
        ASSERT_EQ(4, acc->max);
        ASSERT_EQ(3, acc->label);
        ASSERT_TRUE(func->outputResult(acc, outputTable->allocateRow()));
        ASSERT_NO_FATAL_FAILURE(checkOutputColumn<uint32_t>(outputTable, "label", {3}));
        POOL_DELETE_CLASS(acc);
        DELETE_AND_SET_NULL(func);
    }
    {
        // test invalid input column name
        TablePtr outputTable(new Table(_poolPtr));
        auto func = _creator.createFunction({vt, vt},
                {"a", "b"}, {"label"}, false);
        ASSERT_TRUE(nullptr != func);
        ASSERT_TRUE(func->initResultOutput(outputTable));
        ASSERT_FALSE(func->initMergeInput(table));
        DELETE_AND_SET_NULL(func);
    }
    {
        // test invalid input column type
        TablePtr outputTable(new Table(_poolPtr));
        auto vt2 = ValueTypeHelper<uint16_t>::getValueType();
        auto func = _creator.createFunction({vt2, vt2},
                {"label", "value"}, {"label"}, false);
        ASSERT_TRUE(nullptr != func);
        ASSERT_TRUE(func->initResultOutput(outputTable));
        ASSERT_FALSE(func->initMergeInput(table));
        DELETE_AND_SET_NULL(func);
    }
    {
        // duplicate output column name
        TablePtr outputTable(new Table(_poolPtr));
        outputTable->declareColumn<int>("label");
        auto func = _creator.createFunction({vt, vt},
                {"label", "value"}, {"label"}, false);
        ASSERT_TRUE(nullptr != func);
        ASSERT_FALSE(func->initResultOutput(outputTable));
        DELETE_AND_SET_NULL(func);
    }
}

TEST_F(MaxLabelFuncTest, testGlobalNegative) {
    TablePtr table;
    vector<MatchDoc> docs = std::move(getMatchDocs(_allocator, 3));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(_allocator, docs, "label", {1, 2, 3}));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<int32_t>(_allocator, docs, "value", {-3, -2, -4}));
    table = getTable(createTable(_allocator, docs));
    // test normal
    TablePtr outputTable(new Table(_poolPtr));
    auto func = _creator.createFunction(
            {ValueTypeHelper<uint32_t>::getValueType(), ValueTypeHelper<int32_t>::getValueType()},
            {"label", "value"}, {"label"}, false);
    ASSERT_TRUE(nullptr != func);
    ASSERT_TRUE(func->initResultOutput(outputTable));
    ASSERT_TRUE(func->initMergeInput(table));
    auto acc = static_cast<MaxLabelAccumulator<uint32_t, int32_t> *>(func->createAccumulator(_poolPtr.get()));
    ASSERT_TRUE(func->merge(table->getRow(0), acc));
    ASSERT_EQ(-3, acc->max);
    ASSERT_EQ(1, acc->label);
    ASSERT_TRUE(func->merge(table->getRow(1), acc));
    ASSERT_EQ(-2, acc->max);
    ASSERT_EQ(2, acc->label);
    ASSERT_TRUE(func->merge(table->getRow(2), acc));
    ASSERT_EQ(-2, acc->max);
    ASSERT_EQ(2, acc->label);
    ASSERT_TRUE(func->outputResult(acc, outputTable->allocateRow()));
    ASSERT_NO_FATAL_FAILURE(checkOutputColumn<uint32_t>(outputTable, "label", {2}));
    POOL_DELETE_CLASS(acc);
    DELETE_AND_SET_NULL(func);
}

TEST_F(MaxLabelFuncTest, testGlobalMulti) {
    TablePtr table;
    vector<MatchDoc> docs = std::move(getMatchDocs(_allocator, 3));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(_allocator, docs, "label", {1, 2, 3}));
    ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(_allocator, docs, "value",
                    {{2, 1}, {2, 3}, {2, 3, -1}}));
    table = getTable(createTable(_allocator, docs));
    // test normal
    TablePtr outputTable(new Table(_poolPtr));
    auto func = _creator.createFunction(
            {ValueTypeHelper<uint32_t>::getValueType(), ValueTypeHelper<autil::MultiInt32>::getValueType()},
            {"label", "value"}, {"label"}, false);
    ASSERT_TRUE(nullptr != func);
    ASSERT_TRUE(func->initResultOutput(outputTable));
    ASSERT_TRUE(func->initMergeInput(table));
    auto acc = static_cast<MaxLabelAccumulator<uint32_t, autil::MultiInt32> *>(func->createAccumulator(_poolPtr.get()));
    ASSERT_TRUE(func->merge(table->getRow(0), acc));
    ASSERT_NO_FATAL_FAILURE(checkMultiValue<int32_t>({2, 1}, acc->max));
    ASSERT_EQ(1, acc->label);
    ASSERT_TRUE(func->merge(table->getRow(1), acc));
    ASSERT_NO_FATAL_FAILURE(checkMultiValue<int32_t>({2, 3}, acc->max));
    ASSERT_EQ(2, acc->label);
    ASSERT_TRUE(func->merge(table->getRow(2), acc));
    ASSERT_NO_FATAL_FAILURE(checkMultiValue<int32_t>({2, 3, -1}, acc->max));
    ASSERT_EQ(3, acc->label);
    ASSERT_TRUE(func->outputResult(acc, outputTable->allocateRow()));
    ASSERT_NO_FATAL_FAILURE(checkOutputColumn<uint32_t>(outputTable, "label", {3}));
    POOL_DELETE_CLASS(acc);
    DELETE_AND_SET_NULL(func);
}

END_HA3_NAMESPACE(sql);
