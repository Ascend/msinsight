/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include <gtest/gtest.h>
#include <utility>
#include "FileUtil.h"
#include "TableDefs.h"
#include "ConstantDefs.h"
#include "TestSuit.h"
#include "TextSummaryDataBase.h"
#include "../OperatorLogTestUtil.h"

using namespace Dic;
using namespace Dic::Module::Summary;

std::string g_testDbPath;
std::recursive_mutex g_testMutex;
TextSummaryDataBase g_testDataBase = TextSummaryDataBase(g_testMutex);

class TextSummaryDatabaseTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        g_testDbPath = TestSuit::GetTestDataFile("test_text_database.db");
        g_testDataBase.SetDbPath(g_testDbPath);
        g_testDataBase.OpenDb(g_testDbPath, false);
    }

    static void TearDownTestSuite() {
        g_testDataBase.CloseDb();
        if (FileUtil::CheckFilePathExist(g_testDbPath)) {
            FileUtil::RemoveFile(g_testDbPath);
        }
    }

  protected:
    void SetUp() override {
        g_testDataBase.CreateTable();
        g_testDataBase.InitStmt({});
    }

    void TearDown() override {
        if (!g_testDataBase.IsOpen()) {
            EXPECT_TRUE(g_testDataBase.OpenDb(g_testDbPath, false));
        }
        g_testDataBase.DropAllTable();
        g_testDataBase.ReleaseStmt();
    }
};

TEST_F(TextSummaryDatabaseTest, CreateAndDropTableSuccess) {
    bool result = g_testDataBase.CheckTableExist(TABLE_KERNEL);
    EXPECT_EQ(result, true);
    result = g_testDataBase.DropTable();
    EXPECT_EQ(result, true);
    result = g_testDataBase.CheckTableExist(TABLE_KERNEL);
    EXPECT_EQ(result, false);
    result = g_testDataBase.CreateTable();
    EXPECT_EQ(result, true);
    result = g_testDataBase.CheckTableExist(TABLE_KERNEL);
    EXPECT_EQ(result, true);
}

TEST_F(TextSummaryDatabaseTest, InitAndReleaseStmtSuccess) {
    bool result = g_testDataBase.InitStmt({});
    EXPECT_EQ(result, true);
    result = g_testDataBase.InitStmt({});
    EXPECT_EQ(result, true);
    g_testDataBase.ReleaseStmt();
    // Initial失败暂时未构造出来
}

TEST_F(TextSummaryDatabaseTest, MoreInfoInvalidCurrentLogsSinglePaginationWarn) {
    Protocol::OperatorMoreInfoReqParams params;
    params.rankId = "244";
    params.deviceId = "0";
    params.group = "Communication Operator Type";
    params.opType = "MatMul";
    params.accCore = "AI_CORE";
    params.current = 0;
    params.pageSize = 10;
    Protocol::OperatorMoreInfoResponse response;
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(g_testDataBase.QueryOperatorMoreInfo(params, response));

    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryMoreInfo", "ValidatePagination",
        {"currentPage=0", "cause=currentPage must be greater than zero", "suggestion="},
        {"rankId=", "deviceId=", "group=", "opType=", "accCore=", "pageSize="});
}

TEST_F(TextSummaryDatabaseTest, MoreInfoClosedDatabaseLogsResourceContextOnly) {
    Protocol::OperatorMoreInfoReqParams params = {
        "244", "0", "Operator Type", 15, "SensitiveOpType244", "", "", "SensitiveAccCore244", 1, 10, "", "", {}};
    Protocol::OperatorMoreInfoResponse response;
    g_testDataBase.ReleaseStmt();
    g_testDataBase.CloseDb();
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(g_testDataBase.QueryOperatorMoreInfo(params, response));

    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryMoreInfo", "PrepareSql",
        {"rankId=244", "deviceId=0", "group=Operator Type", "sqliteCode=" + std::to_string(SQLITE_MISUSE),
            "cause=database is closed", "suggestion="},
        {"opType=", "opName=", "inputShape=", "accCore=", "SensitiveOpType244", "SensitiveAccCore244"});
}

TEST_F(TextSummaryDatabaseTest, DetailClosedDatabasePrepareFailureLogsCauseOnce) {
    Protocol::OperatorStatisticReqParams params = {true, "244", "0", "Operator Type", 15, 1, 10, "", ""};
    std::vector<Protocol::OperatorDetailInfoRes> data;
    std::string level;
    g_testDataBase.ReleaseStmt();
    g_testDataBase.CloseDb();
    VirtualSummaryDataBase &database = g_testDataBase;
    const std::vector<std::pair<std::string, std::string>> groups = {{"Operator Type", "group=Operator Type,"},
        {"", "group=,"}, {"Unknown", "group=unknown,"}, {std::string(1024 * 1024, 'x'), "group=unknown,"}};
    for (const auto &group : groups) {
        params.group = group.first;
        const auto mark = OperatorLogTestUtil::Mark();
        EXPECT_FALSE(database.QueryAllOperatorDetailInfo(params, data, level));
        EXPECT_TRUE(params.group == group.first);
        EXPECT_TRUE(data.empty());
        OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryDetail", "PrepareSql",
            {"rankId=244", "deviceId=0", group.second, "sqliteCode=" + std::to_string(SQLITE_MISUSE),
                "cause=database is closed", "suggestion="},
            {"out of memory", "Failed Check Table", "Failed to get Detail Info", "group=Unknown",
                std::string(64, 'x')});
    }
}

TEST_F(TextSummaryDatabaseTest, UnknownCategoryGroupReturnsFailure) {
    Protocol::OperatorDurationReqParams params = {"244", "0", "Unknown", 15};
    std::vector<Protocol::OperatorDurationRes> data;

    for (const auto &group : {std::string("Unknown"), std::string(1024 * 1024, 'x')}) {
        params.group = group;
        const auto mark = OperatorLogTestUtil::Mark();
        EXPECT_FALSE(g_testDataBase.QueryOperatorDurationInfo(params, Protocol::QueryType::CATEGORY, data));
        EXPECT_TRUE(params.group == group);
        EXPECT_TRUE(data.empty());
        OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryCategory", "GenerateSql",
            {"rankId=244", "group=unknown,", "cause=the operator group is unknown", "suggestion="},
            {"group=Unknown", std::string(64, 'x')});
    }
}

TEST_F(TextSummaryDatabaseTest, DetailInvalidOrderByLogsNormalizedGroupOnly) {
    Protocol::OperatorStatisticReqParams params = {false, "244", "0", "Unknown", 15, 1, 10, "name;", "ascend"};
    Protocol::OperatorDetailInfoResponse response;
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_TRUE(g_testDataBase.QueryOperatorDetailInfo(params, response));
    EXPECT_EQ(params.group, "Unknown");
    EXPECT_EQ(params.orderBy, "name;");
    EXPECT_EQ(response.total, 0);
    EXPECT_TRUE(response.data.empty());
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryDetail", "GenerateSql",
        {"rankId=244", "group=unknown,", "cause=orderBy failed SQL validation", "suggestion="},
        {"group=Unknown", "name;"});
}

TEST_F(TextSummaryDatabaseTest, UpdateParseStatusAndCheckSuccess) {
    bool result = g_testDataBase.UpdateParseStatus(NOT_FINISH_STATUS);
    EXPECT_EQ(result, true);
    result = g_testDataBase.HasFinishedParseLastTime();
    EXPECT_EQ(result, false);
    result = g_testDataBase.UpdateParseStatus(FINISH_STATUS);
    EXPECT_EQ(result, true);
    result = g_testDataBase.HasFinishedParseLastTime();
    EXPECT_EQ(result, true);
}

TEST_F(TextSummaryDatabaseTest, SaveKernelDetailAndCheckSuccess) {
    for (int i = 0; i < 1000 / 2; ++i) { // 1000 is cacheSize, 2 is half of cacheSize
        Kernel kernel = {"rank" + std::to_string(i), "step" + std::to_string(i), "task" + std::to_string(i),
            "name" + std::to_string(i), "type" + std::to_string(i), "Dynamic", "AICore", 50 + i, 100.0 + i, 10.0 + i, i,
            "", "", "", "", "", ""};
        g_testDataBase.InsertKernelDetail(kernel, {});
    }
    int64_t num = 1;
    bool result = g_testDataBase.QueryTotalNumByAcceleratorCore("AICore", num);
    EXPECT_EQ(result, true);
    EXPECT_EQ(num, 0);
    g_testDataBase.SaveKernelDetail({});
    result = g_testDataBase.QueryTotalNumByAcceleratorCore("AICore", num);
    EXPECT_EQ(result, true);
    EXPECT_EQ(num, 1000 / 2); // 1000 is cacheSize, 2 is half of cacheSize
}

TEST_F(TextSummaryDatabaseTest, InsertKernelDetailAndCheckSuccess) {
    for (int i = 0; i < 600 / 2; ++i) { // 600 is cacheSize, 2 is half of cacheSize
        Kernel kernel = {"rank" + std::to_string(i), "step" + std::to_string(i), "task" + std::to_string(i),
            "name" + std::to_string(i), "type" + std::to_string(i), "Dynamic", "AICore", 50 + i, 100.0 + i, 10.0 + i, i,
            "", "", "", "", "", ""};
        g_testDataBase.InsertKernelDetail(kernel, {});
    }
    int64_t num = 1;
    bool result = g_testDataBase.QueryTotalNumByAcceleratorCore("AICore", num);
    EXPECT_EQ(result, true);
    EXPECT_EQ(num, 0);
    for (int i = 0; i < 600; ++i) { // 600 is cacheSize, 2 is half of cacheSize
        Kernel kernel = {"rank" + std::to_string(i), "step" + std::to_string(i), "task" + std::to_string(i),
            "name" + std::to_string(i), "type" + std::to_string(i), "Dynamic", "AICore", 50 + i, 100.0 + i, 10.0 + i, i,
            "", "", "", "", "", ""};
        g_testDataBase.InsertKernelDetail(kernel, {});
    }
    result = g_testDataBase.QueryTotalNumByAcceleratorCore("AICore", num);
    EXPECT_EQ(result, true);
    EXPECT_EQ(num, 600); // 600 is cacheSize
    g_testDataBase.SaveKernelDetail({});
}

TEST_F(TextSummaryDatabaseTest, QueryTotalNumByAcceleratorCoreTypeCommunicationOldDataTest) {
    std::vector<Kernel> list = {{"0", "4", "", "aclnnInplaceZero_ZerosLikeAiCore_ZerosLike", "ZerosLike", "dynamic",
                                    "AI_VECTOR", 1695115378722946000, 25.539, 11.041, 40, "", "", "", "", "", ""},
        {"0", "4", "", "aclnnInplaceZero_ZerosLikeAiCore_ZerosLike", "ZerosLike", "dynamic", "AI_VECTOR",
            1695115378713661000, 49.88, 0, 40, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_broadcast__035_0_1", "hcom_broadcast_", "N/A", "HCCL", 1695115378715400200, 4975.2664,
            1242.3992, 0, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_broadcast__035_1_1", "hcom_broadcast_", "N/A", "HCCL", 1695115378715400200, 4975.2664,
            1242.3992, 0, "", "", "", "", "", ""},
        {"0", "4", "", "Slice", "Slice", "dynamic", "AI_CORE", 1695115378723264800, 1.9795, 312.9515, 2, "", "", "", "",
            "", ""}};
    for (const auto &item : list) {
        g_testDataBase.InsertKernelDetail(item, {});
    }
    g_testDataBase.SaveKernelDetail({});
    int64_t total;
    bool result = g_testDataBase.QueryTotalNumByAcceleratorCore("Communication", total);
    ASSERT_TRUE(result);
    EXPECT_EQ(total, 2); // 2
}

TEST_F(TextSummaryDatabaseTest, QueryTotalNumByAcceleratorCoreTypeCommunicationNewDataTest) {
    std::vector<Kernel> list = {
        {"0", "4", "", "aclnnInplaceZero_ZerosLikeAiCore_ZerosLike", "ZerosLike", "dynamic", "AI_VECTOR",
            1695115378722946000, 25.539, 11.041, 40, "", "", "", "", "", ""},
        {"0", "4", "", "aclnnInplaceZero_ZerosLikeAiCore_ZerosLike", "ZerosLike", "dynamic", "AI_VECTOR",
            1695115378713661000, 49.88, 0, 40, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_allReduce__491_4_1", "hcom_allReduce_", "N/A", "COMMUNICATION", 1695115378715400200,
            4975.2664, 1242.3992, 0, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_allGather__491_5_1", "hcom_allGather_", "N/A", "COMMUNICATION", 1695115378715400200,
            4975.2664, 1242.3992, 0, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_reduceScatter__491_6_1", "hcom_reduceScatter_", "N/A", "COMMUNICATION",
            1695115378715400200, 4975.2664, 1242.3992, 0, "", "", "", "", "", ""},
    };
    for (const auto &item : list) {
        g_testDataBase.InsertKernelDetail(item, {});
    }
    g_testDataBase.SaveKernelDetail({});
    int64_t total;
    bool result = g_testDataBase.QueryTotalNumByAcceleratorCore("Communication", total);
    ASSERT_TRUE(result);
    EXPECT_EQ(total, 3); // 3
}

TEST_F(TextSummaryDatabaseTest, QueryCommunicationOpDetailOldDataTest) {
    std::vector<Kernel> list = {{"0", "4", "", "aclnnInplaceZero_ZerosLikeAiCore_ZerosLike", "ZerosLike", "dynamic",
                                    "AI_VECTOR", 1695115378722946000, 25.539, 11.041, 40, "", "", "", "", "", ""},
        {"0", "4", "", "aclnnInplaceZero_ZerosLikeAiCore_ZerosLike", "ZerosLike", "dynamic", "AI_VECTOR",
            1695115378713661000, 49.88, 0, 40, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_broadcast__035_0_1", "hcom_broadcast_", "N/A", "HCCL", 1695115378715400200, 4975.2664,
            1242.3992, 0, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_broadcast__035_1_1", "hcom_broadcast_", "N/A", "HCCL", 1695115378715400200, 4975.2664,
            1242.3992, 0, "", "", "", "", "", ""},
        {"0", "4", "", "Slice", "Slice", "dynamic", "AI_CORE", 1695115378723264800, 1.9795, 312.9515, 2, "", "", "", "",
            "", ""}};
    for (const auto &item : list) {
        g_testDataBase.InsertKernelDetail(item, {});
    }
    g_testDataBase.SaveKernelDetail({});
    CommunicationDetailParams params;
    params.pageSize = 10; // 10
    params.currentPage = 1;
    std::vector<Protocol::CommunicationDetail> computeDetails;
    bool result = g_testDataBase.QueryCommunicationOpDetail(params, computeDetails);
    ASSERT_TRUE(result);
    ASSERT_EQ(computeDetails.size(), 2); // 2
    EXPECT_EQ(computeDetails[0].name, "hcom_broadcast__035_0_1");
    EXPECT_EQ(computeDetails[1].name, "hcom_broadcast__035_1_1");
}

TEST_F(TextSummaryDatabaseTest, QueryCommunicationOpDetailNewDataTest) {
    std::vector<Kernel> list = {
        {"0", "4", "", "aclnnInplaceZero_ZerosLikeAiCore_ZerosLike", "ZerosLike", "dynamic", "AI_VECTOR",
            1695115378722946000, 25.539, 11.041, 40, "", "", "", "", "", ""},
        {"0", "4", "", "aclnnInplaceZero_ZerosLikeAiCore_ZerosLike", "ZerosLike", "dynamic", "AI_VECTOR",
            1695115378713661000, 49.88, 0, 40, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_allReduce__491_4_1", "hcom_allReduce_", "N/A", "COMMUNICATION", 1695115378715400200,
            4975.2664, 1242.3992, 0, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_allGather__491_5_1", "hcom_allGather_", "N/A", "COMMUNICATION", 1695115378715400200,
            4975.2664, 1242.3992, 0, "", "", "", "", "", ""},
        {"0", "4", "", "hcom_reduceScatter__491_6_1", "hcom_reduceScatter_", "N/A", "COMMUNICATION",
            1695115378715400200, 4975.2664, 1242.3992, 0, "", "", "", "", "", ""},
    };
    for (const auto &item : list) {
        g_testDataBase.InsertKernelDetail(item, {});
    }
    g_testDataBase.SaveKernelDetail({});
    CommunicationDetailParams params;
    params.pageSize = 10; // 10
    params.currentPage = 1;
    std::vector<Protocol::CommunicationDetail> computeDetails;
    bool result = g_testDataBase.QueryCommunicationOpDetail(params, computeDetails);
    ASSERT_TRUE(result);
    ASSERT_EQ(computeDetails.size(), 3); // 3
    EXPECT_EQ(computeDetails[0].name, "hcom_allReduce__491_4_1");
    EXPECT_EQ(computeDetails[1].name, "hcom_allGather__491_5_1");
    EXPECT_EQ(computeDetails[2].name, "hcom_reduceScatter__491_6_1"); // 2
}

TEST_F(TextSummaryDatabaseTest, QueryMinStartTimeTest) {
    uint64_t start = g_testDataBase.QueryMinStartTime();
    EXPECT_EQ(start, INT64_MAX);
    std::vector<Kernel> list = {{"0", "2", "", "Cast", "Cast", "Dynamic", "AI_CORE", 1695115378722946000, 5.7985,
                                    11.041, 33, "", "", "", "", "", ""},
        {"0", "2", "", "ZerosLike", "ZerosLike", "Dynamic", "AI_CORE", 1695115378713661000, 496.8508, 0, 48, "", "", "",
            "", "", ""},
        {"0", "2", "", "hcom_broadcast__483_0", "hcom_broadcast_", "NA", "HCCL", 1695115378715400200, 4975.2664,
            1242.3992, 0, "", "", "", "", "", ""},
        {"0", "2", "", "Slice", "Slice", "Dynamic", "AI_CORE", 1695115378723264800, 1.9795, 312.9515, 2, "", "", "", "",
            "", ""},
        {"0", "2", "", "hcom_broadcast__483_1", "hcom_broadcast_", "NA", "HCCL", 1695115378722392500, 4975.2664,
            1242.3992, 0, "", "", "", "", "", ""}};
    for (const auto &item : list) {
        g_testDataBase.InsertKernelDetail(item, {});
    }
    g_testDataBase.SaveKernelDetail({});
    start = g_testDataBase.QueryMinStartTime();
    EXPECT_EQ(start, 1695115378713661000); // 1695115378713661000
}

TEST_F(TextSummaryDatabaseTest, QueryRankIdsTest) {
    auto ranks = g_testDataBase.QueryRankIds();
    EXPECT_EQ(ranks.size(), 0);
    std::vector<Kernel> list = {{"0", "2", "", "Cast", "Cast", "Dynamic", "AI_CORE", 1695115378722946000, 5.7985,
                                    11.041, 33, "", "", "", "", "", ""},
        {"1", "2", "", "ZerosLike", "ZerosLike", "Dynamic", "AI_CORE", 1695115378713661000, 496.8508, 0, 48, "", "", "",
            "", "", ""},
        {"2", "2", "", "hcom_broadcast__483_0", "hcom_broadcast_", "NA", "HCCL", 1695115378715400200, 4975.2664,
            1242.3992, 0, "", "", "", "", "", ""}};
    for (const auto &item : list) {
        g_testDataBase.InsertKernelDetail(item, {});
    }
    g_testDataBase.SaveKernelDetail({});
    ranks = g_testDataBase.QueryRankIds();
    EXPECT_EQ(ranks.size(), list.size());
}
