#include <ha3/sql/ops/test/OpTestBase.h>
#include <ha3/sql/ops/join/JoinKernelBase.h>
#include <ha3/sql/data/TableSchema.h>
#include <autil/mem_pool/Pool.h>
using namespace std;
using namespace matchdoc;
using namespace suez::turing;
using namespace navi;
using namespace autil;
using namespace autil::legacy;
using namespace autil::legacy::json;

BEGIN_HA3_NAMESPACE(sql);

class JoinKernelBaseTest : public OpTestBase {
public:
    JoinKernelBaseTest();
    ~JoinKernelBaseTest();
public:
    void setUp() override {
        prepareAttribute();
    }
    void tearDown() override {
        _attributeMap.clear();
    }
private:
    void prepareAttribute() {
        _attributeMap["join_type"] = string("INNER");
        _attributeMap["left_input_fields"] = ParseJson(R"json(["$uid", "$cid"])json");
        _attributeMap["right_input_fields"] = ParseJson(R"json(["$cid", "$group_name"])json");
        _attributeMap["system_field_num"] = Any(0);
        _attributeMap["output_fields"] =
            ParseJson(R"json(["$uid", "$cid", "$cid0", "$group_name0"])json");
        _attributeMap["equi_condition"] = _attributeMap["condition"] =
            ParseJson(R"json({"op":"=", "type":"OTHER", "params":["$cid0", "$cid"]})json");
    }

    void setResource(KernelTesterBuilder &testerBuilder) {
        for (auto &resource : _resourceMap) {
            testerBuilder.resource(resource.first, resource.second);
        }
    }

    KernelTesterPtr buildTester(KernelTesterBuilder builder) {
        setResource(builder);
        builder.kernel("JoinKernelBase");
        builder.input("input0");
        builder.input("input1");
        builder.output("output0");
        builder.attrs(autil::legacy::ToJsonString(_attributeMap));
        return KernelTesterPtr(builder.build());
    }
    void checkColumn(const TablePtr &table, string columnName, BuiltinType type, bool isMulti) {
        ColumnPtr tableColumn = table->getColumn(columnName);
        ASSERT_TRUE(tableColumn != NULL);
        auto schema = tableColumn->getColumnSchema();
        ASSERT_TRUE(schema != NULL);
        auto vt = schema->getType();
        ASSERT_EQ(type, vt.getBuiltinType());
        ASSERT_EQ(isMulti, vt.isMultiValue());
    }

    void checkHashValuesOffset(JoinKernelBase::HashValues &hashValues, JoinKernelBase::HashValues expectValues) {
        ASSERT_EQ(hashValues.size(), expectValues.size());
        for (size_t i = 0; i < hashValues.size(); i++) {
            EXPECT_EQ(expectValues[i], hashValues[i]);
        }
    }

};

JoinKernelBaseTest::JoinKernelBaseTest() {
}

JoinKernelBaseTest::~JoinKernelBaseTest() {
}

TEST_F(JoinKernelBaseTest, testJoinKeyInSameTable) {
    string anyUdfStr= R"json({"op":"ANY", "params":["$cid"], "type":"UDF"})json";
    string condition = "{\"op\":\"=\", \"params\":[" + anyUdfStr + ", \"$cid\"]}";
    _attributeMap["equi_condition"] = _attributeMap["condition"] = ParseJson(condition);
    auto testerPtr = buildTester(KernelTesterBuilder());
    ASSERT_TRUE(testerPtr.get());
    ASSERT_TRUE(testerPtr->hasError());
}

TEST_F(JoinKernelBaseTest, testInputOutputNotSame) {
    _attributeMap["left_input_fields"] = ParseJson(R"json(["$uid", "$cid"])json");
    _attributeMap["right_input_fields"] = ParseJson(R"json(["$cid", "$group_name"])json");
    _attributeMap["output_fields"] =
        ParseJson(R"json(["$uid", "$cid0", "$group_name0"])json");
    auto testerPtr = buildTester(KernelTesterBuilder());
    ASSERT_TRUE(testerPtr.get());
    ASSERT_TRUE(testerPtr->hasError());
}


TEST_F(JoinKernelBaseTest, testInputOutputNotSame2) {
    _attributeMap["left_input_fields"] = ParseJson(R"json(["$uid", "$cid"])json");
    _attributeMap["right_input_fields"] = ParseJson(R"json(["$cid", "$group_name"])json");
    _attributeMap["output_fields"] =
        ParseJson(R"json(["$uid","$cid", "$cid0","$group_name0", "$group_name1"])json");
    auto testerPtr = buildTester(KernelTesterBuilder());
    ASSERT_TRUE(testerPtr.get());
    ASSERT_TRUE(testerPtr->hasError());
}

TEST_F(JoinKernelBaseTest, testSystemFieldNotZero) {
    _attributeMap["system_field_num"] = Any(1);
    auto testerPtr = buildTester(KernelTesterBuilder());
    ASSERT_TRUE(testerPtr.get());
    ASSERT_TRUE(testerPtr->hasError());
}


TEST_F(JoinKernelBaseTest, testJoinMultiFieldFailed) {
    string anyUdfStr= R"json({"op":"ANY", "params":["$cid"], "type":"UDF"})json";
    string condition = "{\"op\":\"=\", \"params\":[" + anyUdfStr + ", " + anyUdfStr + "]}";
    _attributeMap["equi_condition"] = _attributeMap["condition"] = ParseJson(condition);
    auto testerPtr = buildTester(KernelTesterBuilder());
    ASSERT_TRUE(testerPtr.get());
    ASSERT_TRUE(testerPtr->hasError());
}

TEST_F(JoinKernelBaseTest, testConvertFields) {
    {
        JoinKernelBase joinBase;
        joinBase._systemFieldNum = 1;
        ASSERT_FALSE(joinBase.convertFields());
    }
    {
        JoinKernelBase joinBase;
        ASSERT_TRUE(joinBase.convertFields());
    }
    {
        JoinKernelBase joinBase;
        joinBase._leftInputFields = {"1", "2"};
        ASSERT_FALSE(joinBase.convertFields());
        joinBase._rightInputFields = {"3"};
        joinBase._outputFields = {"1", "2", "30"};
        ASSERT_TRUE(joinBase.convertFields());
        ASSERT_EQ("1", joinBase._output2InputMap["1"].first);
        ASSERT_TRUE(joinBase._output2InputMap["1"].second);
        ASSERT_EQ("2", joinBase._output2InputMap["2"].first);
        ASSERT_TRUE(joinBase._output2InputMap["2"].second);
        ASSERT_EQ("3", joinBase._output2InputMap["30"].first);
        ASSERT_FALSE(joinBase._output2InputMap["30"].second);
    }
}


TEST_F(JoinKernelBaseTest, testCombineHashValues) {
    {
        JoinKernelBase::HashValues left = {{1,10}, {2, 22}, {3, 33}};
        JoinKernelBase::HashValues right = {{1,100}, {2, 122}, {3, 133}};
        JoinKernelBase joinBase;
        joinBase.combineHashValues(left, right);
        ASSERT_EQ(3, right.size());
    }
    {
        JoinKernelBase::HashValues left = {};
        JoinKernelBase::HashValues right = {{1,100}, {2, 122}, {3, 133}};
        JoinKernelBase joinBase;
        joinBase.combineHashValues(left, right);
        ASSERT_EQ(0, right.size());
    }
    {
        JoinKernelBase::HashValues left = {{1,10}, {2, 22}, {3, 33}};
        JoinKernelBase::HashValues right = {};
        JoinKernelBase joinBase;
        joinBase.combineHashValues(left, right);
        ASSERT_EQ(0, right.size());
    }
    {
        JoinKernelBase::HashValues left = {{1,10}, {3, 22}, {5, 33}};
        JoinKernelBase::HashValues right = {{2,100}, {4, 122}, {6, 133}};
        JoinKernelBase joinBase;
        joinBase.combineHashValues(left, right);
        ASSERT_EQ(0, right.size());
    }
    {
        JoinKernelBase::HashValues left = {{1,10}, {1, 22}, {1, 33}};
        JoinKernelBase::HashValues right = {{1,100}, {1, 122}, {3, 133}};
        JoinKernelBase joinBase;
        joinBase.combineHashValues(left, right);
        ASSERT_EQ(6, right.size());
    }
    {
        JoinKernelBase::HashValues left = {{2,10}};
        JoinKernelBase::HashValues right = {{1,100}, {2, 22}, {3, 33}};
        JoinKernelBase joinBase;
        joinBase.combineHashValues(left, right);
        ASSERT_EQ(1, right.size());
    }
}

TEST_F(JoinKernelBaseTest, testCreateHashMap) {
    {
        JoinKernelBase base;
        base._leftJoinColumns = {"cid"};
        base._rightJoinColumns = {"cid"};
        MatchDocAllocatorPtr allocator;
        vector<MatchDoc> leftDocs = std::move(getMatchDocs(allocator, 2));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator(allocator, leftDocs, "cid", {"1111", "3333"}));
        TablePtr inputTable(new Table(leftDocs, allocator));
        ASSERT_TRUE(base.createHashMap(inputTable, 0, inputTable->getRowCount(), true));
        ASSERT_EQ(2, base._hashJoinMap.size());
    }
    {
        JoinKernelBase base;
        base._leftJoinColumns = {"muid", "cid", "mcid"};
        base._rightJoinColumns = {"muid", "cid", "mcid"};
        MatchDocAllocatorPtr allocator;
        vector<MatchDoc> leftDocs = std::move(getMatchDocs(allocator, 2));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(allocator, leftDocs, "uid", {0, 1}));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator(allocator, leftDocs, "cid", {"1111", "3333"}));
        ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "muid", {{1, 3}, {2}}));
        ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "mcid", {{1, 3, 4}, {2, 4, 5, 6}}));
        TablePtr inputTable(new Table(leftDocs, allocator));
        ASSERT_TRUE(base.createHashMap(inputTable, 0, inputTable->getRowCount(), true));
        ASSERT_EQ(10, base._hashJoinMap.size());
    }
    {
        JoinKernelBase base;
        base._leftJoinColumns = {"muid", "cid", "mcid"};
        base._rightJoinColumns = {"muid", "cid", "mcid"};
        MatchDocAllocatorPtr allocator;
        vector<MatchDoc> leftDocs = std::move(getMatchDocs(allocator, 2));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(allocator, leftDocs, "uid", {0, 1}));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator(allocator, leftDocs, "cid", {"1111", "3333"}));
        ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "muid", {{1, 3}, {2}}));
        ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "mcid", {{1, 3, 4}, {2, 4, 2, 2}}));
        TablePtr inputTable(new Table(leftDocs, allocator));
        ASSERT_TRUE(base.createHashMap(inputTable, 0, inputTable->getRowCount(), true));
        ASSERT_EQ(8, base._hashJoinMap.size());
    }
    { //multi value has empty
        JoinKernelBase base;
        base._leftJoinColumns = {"muid", "cid", "mcid"};
        base._rightJoinColumns = {"muid", "cid", "mcid"};
        MatchDocAllocatorPtr allocator;
        vector<MatchDoc> leftDocs = std::move(getMatchDocs(allocator, 2));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(allocator, leftDocs, "uid", {0, 1}));
        ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator(allocator, leftDocs, "cid", {"1111", "3333"}));
        ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "muid", {{1, 3}, {2}}));
        ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "mcid", {{1, 3, 4}, {}}));
        TablePtr inputTable(new Table(leftDocs, allocator));
        ASSERT_TRUE(base.createHashMap(inputTable, 0, inputTable->getRowCount(), true));
        ASSERT_EQ(6, base._hashJoinMap.size());
    }
}

TEST_F(JoinKernelBaseTest, testGetColumnHashValues) {
    MatchDocAllocatorPtr allocator;
    vector<MatchDoc> leftDocs = std::move(getMatchDocs(allocator, 3));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(allocator, leftDocs, "uid", {0, 1, 2}));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(allocator, leftDocs, "uid2", {2, 1, 0}));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator(allocator, leftDocs, "cid", {"1111", "3333", "1111"}));
    ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "muid", {{1, 3}, {2}, {4}}));
    ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "mcid", {{1, 3, 4}, {2, 4, 5, 6}, {}}));
    TablePtr inputTable(new Table(leftDocs, allocator));
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getColumnHashValues(inputTable, 0, inputTable->getRowCount(), "uid", hashValues));
        ASSERT_FALSE(base._shouldClearTable);
        checkHashValuesOffset(hashValues, { {0, 0}, {1, 1}, {2, 2}});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getColumnHashValues(inputTable, 0, 1, "uid", hashValues));
        ASSERT_FALSE(base._shouldClearTable);
        checkHashValuesOffset(hashValues, {{0, 0}});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getColumnHashValues(inputTable, 1, 2, "uid", hashValues));
        ASSERT_FALSE(base._shouldClearTable);
        checkHashValuesOffset(hashValues, {{1, 1}, {2, 2}});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getColumnHashValues(inputTable, 2, 4, "uid", hashValues));
        ASSERT_FALSE(base._shouldClearTable);
        checkHashValuesOffset(hashValues, {{2, 2}});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getColumnHashValues(inputTable, 5, 4, "uid", hashValues));
        ASSERT_TRUE(base._shouldClearTable);
        checkHashValuesOffset(hashValues, {});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getColumnHashValues(inputTable, 0, 4, "mcid", hashValues));
        ASSERT_FALSE(base._shouldClearTable);
        checkHashValuesOffset(hashValues, {{0, 1}, {0, 3}, {0, 4}, {1, 2}, {1, 4}, {1, 5}, {1, 6}});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getColumnHashValues(inputTable, 2, 1, "mcid", hashValues));
        ASSERT_TRUE(base._shouldClearTable);
        checkHashValuesOffset(hashValues, {});
    }
}

TEST_F(JoinKernelBaseTest, testGetHashValues) {
    MatchDocAllocatorPtr allocator;
    vector<MatchDoc> leftDocs = std::move(getMatchDocs(allocator, 3));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(allocator, leftDocs, "uid", {0, 1, 2}));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator<uint32_t>(allocator, leftDocs, "uid2", {2, 1, 0}));
    ASSERT_NO_FATAL_FAILURE(extendMatchDocAllocator(allocator, leftDocs, "cid", {"1111", "3333", "1111"}));
    ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "muid", {{1, 3}, {2}, {4}}));
    ASSERT_NO_FATAL_FAILURE(extendMultiValueMatchDocAllocator<int32_t>(allocator, leftDocs, "mcid", {{1, 3, 4}, {2, 4, 5, 6}, {}}));
    TablePtr inputTable(new Table(leftDocs, allocator));
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getHashValues(inputTable, 0, 3, {"uid"}, hashValues));
        checkHashValuesOffset(hashValues, { {0, 0}, {1, 1}, {2, 2}});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getHashValues(inputTable, 0, 3, {"uid", "uid2"}, hashValues));
        checkHashValuesOffset(hashValues, { {0, 2654435899}, {1, 2654435835}, {2, 2654435771}});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getHashValues(inputTable, 0, 3, {"uid", "cid"}, hashValues));
        checkHashValuesOffset(hashValues, { {0, 14782428492426752116ul}, {1, 17318972269798604881ul},
                                                                       {2, 14782428492426752118ul}});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getHashValues(inputTable, 0, 3, {"mcid"}, hashValues));
        checkHashValuesOffset(hashValues, { {0, 1}, {0, 3}, {0, 4}, {1, 2}, {1, 4}, {1, 5}, {1, 6}});
    }
    {
        JoinKernelBase base;
        JoinKernelBase::HashValues hashValues;
        ASSERT_TRUE(base.getHashValues(inputTable, 0, 3, {"mcid", "muid"}, hashValues));
        checkHashValuesOffset(hashValues, { {0, 2654435835}, {0, 2654435837}, {0, 2654435836}, {0, 2654435961},
                             {0, 2654435967}, {0, 2654435966}, {1, 2654435897}, {1, 2654435903},
                             {1, 2654435900}, {1, 2654435901}});
    }


}

END_HA3_NAMESPACE(sql);
