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
#include "FileUtil.h"
#include "ParamsParser.h"
#include "DataBaseManager.h"
#include "JsonFileParserManager.h"
#include "KernelParse.h"
#include "ParserStatusManager.h"
#include "TraceTime.h"
#include "SystemViewOverallRepoFactory.h"
#include "TraceDatabaseSqlConst.h"

using namespace Dic::Module::Summary;
using namespace Dic::Server;
using namespace Dic;
namespace Dic::Module::Timeline {
class SystemViewOverallTextRepoTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        // Repo Factory
        SystemViewOverallRepoFactory::Instance();
        // database
        std::string currPath = Dic::FileUtil::GetCurrPath();
        const ParamsOption &option = ParamsParser::Instance().GetOption();
        ServerLog::Initialize(option.logPath, option.logSize, option.logLevel, to_string(option.wsPort));
        std::string subStr = "server";
        size_t index = currPath.rfind(subStr);
        currPath = currPath.substr(0, index - 1);
        std::string refPath0 = FileUtil::SplicePath(
            "test", "data", "pytorch", "text", "level1", "rank0_ascend_pt", "ASCEND_PROFILER_OUTPUT");
        std::string refPathWithoutPMU = FileUtil::SplicePath(
            "test", "data", "pytorch", "text", "level0", "rank1_ascend_pt", "ASCEND_PROFILER_OUTPUT");
        std::string refPath0Db = Dic::FileUtil::SplicePath(currPath, refPath0, "mindstudio_insight_data.db");
        std::string refPathWithoutPMUDb =
            Dic::FileUtil::SplicePath(currPath, refPathWithoutPMU, "mindstudio_insight_data.db");
        DataBaseManager::Instance().SetDataType(DataType::TEXT, refPath0Db);
        DataBaseManager::Instance().SetFileType(FileType::PYTORCH, refPath0Db);
        DataBaseManager::Instance().SetDataType(DataType::TEXT, refPathWithoutPMUDb);
        DataBaseManager::Instance().SetFileType(FileType::PYTORCH, refPathWithoutPMUDb);
        DataBaseManager::Instance().CreateTraceConnectionPool("0", refPath0Db);
        DataBaseManager::Instance().CreateTraceConnectionPool("1", refPathWithoutPMUDb);
        DataBaseManager::Instance().UpdateRankIdToDeviceId(refPath0Db, "0", "0");
        DataBaseManager::Instance().UpdateRankIdToDeviceId(refPathWithoutPMUDb, "1", "1");
        JsonFileParserManager::GetTraceFileParser().Parse(
            {Dic::FileUtil::SplicePath(currPath, refPath0, "trace_view.json")}, "0", "", refPath0Db);
        WaitParseEnd({"0"});
        JsonFileParserManager::GetTraceFileParser().Parse(
            {Dic::FileUtil::SplicePath(currPath, refPathWithoutPMU, "trace_view.json")}, "1", "", refPathWithoutPMUDb);
        WaitParseEnd({"1"});
        std::string testDataPath0 =
            Dic::FileUtil::SplicePath(currPath, "test", "data", "pytorch", "text", "level1", "rank0_ascend_pt");
        KernelParse::Instance().Parse({refPath0Db, "0", testDataPath0});
        WaitParseEnd({KERNEL_PREFIX + "0"});
        std::string testDataPathWithoutPmu =
            Dic::FileUtil::SplicePath(currPath, "test", "data", "pytorch", "text", "level0", "rank1_ascend_pt");
        KernelParse::Instance().Parse({refPathWithoutPMUDb, "1", testDataPathWithoutPmu});
        WaitParseEnd({KERNEL_PREFIX + "1"});
    }

    static void WaitParseEnd(std::vector<std::string> statusList) {
        while (true) {
            size_t i = 0;
            for (const auto &tmp : statusList) {
                if (ParserStatusManager::Instance().GetParserStatus(tmp) != ParserStatus::FINISH) {
                    break;
                } else {
                    i++;
                }
            }
            if (i < statusList.size()) {
                continue;
            } else {
                Dic::Server::ServerLog::Info("parse end");
                return;
            }
        }
    }

    static void TearDownTestSuite() {
        SystemViewOverallRepoFactory::Instance()->Reset();
        KernelParse::Instance().Reset();
        TraceFileParser::DeleteParseFiles({"0"});
        TraceFileParser::DeleteParseFiles({"1"});
    }

    static int Main(int argc, char **argv) {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
};

TEST_F(SystemViewOverallTextRepoTest, QueryOverlapAnalysisDataForOverallMetricTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    auto repoPtr = SystemViewOverallRepoFactory::Instance()->GetSystemViewOverallRepo(
        DataBaseManager::Instance().GetDataType(database->GetDbPath()));
    if (repoPtr == nullptr) {
        // GetSystemViewOverallRepo中已有日志报错
        return;
    }
    Dic::Protocol::SystemViewOverallReqParam requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    std::vector<OverallTmpInfo> overlapInfos;
    overlapInfos = repoPtr->QueryOverlapAnalysisDataForOverallMetric(requestParams, database);
    ASSERT_EQ(overlapInfos.size(), 3); // 3
    const double toleranceThreshold = 0.01;
    const std::vector<std::string> EXPECT_OVERLAP_NAME = {"Communication(Not Overlapped)", "Computing", "Free"};
    const std::vector<double> EXPECT_OVERLAP_TINE = {101963.2, 8873.76, 25744.05};
    for (size_t index = 0; index < EXPECT_OVERLAP_TINE.size(); index++) {
        ASSERT_FALSE(overlapInfos[index].categoryList.empty());
        EXPECT_EQ(overlapInfos[index].categoryList[0], EXPECT_OVERLAP_NAME[index]);
        EXPECT_NEAR(overlapInfos[index].duration, EXPECT_OVERLAP_TINE[index], toleranceThreshold);
    }
}

// System View Overall: 查询Computing拆解所需数据（有PMU数据，能正常查询）
TEST_F(SystemViewOverallTextRepoTest, QueryDataForComputingOverallMetricTestWithPmu) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    auto repoPtr = SystemViewOverallRepoFactory::Instance()->GetSystemViewOverallRepo(
        DataBaseManager::Instance().GetDataType(database->GetDbPath()));
    if (repoPtr == nullptr) {
        // GetSystemViewOverallRepo中已有日志报错
        return;
    }
    Dic::Protocol::SystemViewOverallReqParam requestParams;
    std::vector<SystemViewOverallRes> details;
    Protocol::SystemViewOverallRes tmpRes = {.totalTime = 8873.76,
        .ratio = 6.5, // Computing Time = 8873.76, 6.5%
        .nums = 0,
        .avg = 0,
        .max = 0,
        .min = UINT32_MAX,
        .name = COMPUTING_TIME,
        .children = {},
        .level = 1};
    details.emplace_back(tmpRes);
    tmpRes = {.totalTime = 101963.2,
        .ratio = 74.65, // Comm(Not Overlapped) = 101963.2, 74.65%
        .nums = 0,
        .avg = 0,
        .max = 0,
        .min = 0,
        .name = COMMUNICATION_NOT_OVERLAP_TIME,
        .children = {},
        .level = 1};
    details.emplace_back(tmpRes);
    tmpRes = {.totalTime = 25744.05,
        .ratio = 18.85, // Free = 25744.05, 18.85%
        .nums = 0,
        .avg = 0,
        .max = 0,
        .min = 0,
        .name = FREE_TIME,
        .children = {},
        .level = 1};
    details.emplace_back(tmpRes);
    tmpRes = {.totalTime = 136581.01,
        .ratio = 100, // E2E = 136581.01, 100%
        .nums = 0,
        .avg = 0,
        .max = 0,
        .min = 0,
        .name = E2E_TIME,
        .children = {},
        .level = 1};
    details.emplace_back(tmpRes);
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    SystemViewOverallHelper computeHelper;
    bool result = repoPtr->QueryDataForComputingOverallMetric(requestParams, computeHelper, database);
    EXPECT_EQ(result, true);
    EXPECT_EQ(computeHelper.cpuCubeOps.size(), 24); // 24
    EXPECT_EQ(computeHelper.kernelEvents.size(), 185); // 185
    EXPECT_EQ(computeHelper.bwdTrackId, 2); // 2
    computeHelper.CategorizeComputingEvents();
    computeHelper.AggregateComputingOverallMetrics(details);
    EXPECT_EQ(details.size(), 4); // 4
    EXPECT_EQ(details[0].children.size(), 5); // 5
    // more details
    std::vector<std::string> tempList = {"Conv"};
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::vector<SameOperatorsDetails> filteredEvents =
        computeHelper.FilterComputingEventsByCategory(tempList, minTimestamp, "");
    EXPECT_EQ(filteredEvents.size(), 66); // 66个Conv类算子
}

// System View Overall: 累加计算溢出时防护
TEST_F(SystemViewOverallTextRepoTest, AggregateComputingOverallMetricsOverflow) {
    std::vector<SystemViewOverallRes> details;
    Protocol::SystemViewOverallRes tmpRes =
        {.totalTime = 8873.76,
            .ratio = 6.5, // Computing Time = 8873.76, 6.5%
            .nums = 0,
            .avg = 0,
            .max = 0,
            .min = UINT32_MAX,
            .name = OVERALL_CAT_COMPUTING,
            .children =
                {
                    SystemViewOverallRes{.totalTime = 8873.76,
                        .ratio = 6.5,
                        .nums = 0,
                        .avg = 0,
                        .max = 0,
                        .min = UINT32_MAX,
                        .name = OVERALL_CAT_COMPUTING,
                        .children = {SystemViewOverallRes{.totalTime = 8873.76,
                                         .ratio = 6.5,
                                         .nums = 1,
                                         .avg = 0,
                                         .max = 0,
                                         .min = UINT32_MAX,
                                         .name = "A-1"},
                            SystemViewOverallRes{.totalTime = 8873.76,
                                .ratio = 6.5,
                                .nums = 1,
                                .avg = 0,
                                .max = 0,
                                .min = UINT32_MAX,
                                .name = "A-2"}}},
                    SystemViewOverallRes{.totalTime = 8873.76,
                        .ratio = 6.5,
                        .nums = 1,
                        .avg = 0,
                        .max = 0,
                        .min = UINT32_MAX,
                        .name = OVERALL_CAT_COMPUTING,
                        .children = {SystemViewOverallRes{.totalTime = 8873.76,
                                         .ratio = 6.5,
                                         .nums = 1,
                                         .avg = 0,
                                         .max = 0,
                                         .min = UINT32_MAX,
                                         .name = "B-1"},
                            SystemViewOverallRes{.totalTime = 8873.76,
                                .ratio = 6.5,
                                .nums = UINT32_MAX - 1,
                                .avg = 0,
                                .max = 0,
                                .min = UINT32_MAX,
                                .name = "B-2"}}},
                },
            .level = 1};
    details.emplace_back(tmpRes);
    SystemViewOverallHelper computeHelper;
    computeHelper.AggregateComputingOverallMetrics(details);
    EXPECT_EQ(details.size(), 1); // 1
    EXPECT_EQ(details[0].nums, 1); // 2
    EXPECT_EQ(details[0].children.size(), 2); // 2
    EXPECT_EQ(details[0].children[0].nums, 2); // 2
    EXPECT_EQ(details[0].children[1].nums, UINT32_MAX); // 2
}

// System View Overall: 查询过滤 duration > UINT64_MAX / 1000 的值
TEST_F(SystemViewOverallTextRepoTest, QueryDataForComputingOverallMetricTestFilterDurationIsTooLong) {
    SystemViewOverallHelper computeHelper;
    std::vector<std::string> tempList = {"Conv"};
    const double range = static_cast<double>(INT64_MAX) / 1000.0;
    computeHelper.kernelEvents = {
        OverallTmpInfo{.opName = "Conv", .startTime = 0, .duration = 10, .categoryList = tempList},
        OverallTmpInfo{.opName = "Conv", .startTime = 0, .duration = range, .categoryList = tempList},
        OverallTmpInfo{.opName = "Conv", .startTime = 0, .duration = range + 1.0f, .categoryList = tempList},
        OverallTmpInfo{.opName = "Conv",
            .startTime = 0,
            .duration = range + 2.0f,
            .categoryList = tempList}, // UINT64_MAX 转 double 精度丢失 2，到这里 duration 都小于 UINT64_MAX / 1000
        OverallTmpInfo{.opName = "Conv", .startTime = 0, .duration = range + 3.0f, .categoryList = tempList},
        OverallTmpInfo{.opName = "Conv",
            .startTime = 0,
            .duration = static_cast<long double>(INT64_MAX),
            .categoryList = tempList},
    };
    uint64_t minTimestamp = 0;
    std::vector<SameOperatorsDetails> filteredEvents =
        computeHelper.FilterComputingEventsByCategory(tempList, minTimestamp, "Conv");
#if defined(__APPLE__)
    EXPECT_EQ(filteredEvents.size(), 3);
#else
    EXPECT_EQ(filteredEvents.size(), 1);
#endif
}

//  System View Overall: 查询Computing拆解所需数据（无PMU数据，无法进行Computing拆解）
TEST_F(SystemViewOverallTextRepoTest, QueryDataForComputingOverallMetricTestWithoutPmu) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("1");
    auto repoPtr = SystemViewOverallRepoFactory::Instance()->GetSystemViewOverallRepo(
        DataBaseManager::Instance().GetDataType(database->GetDbPath()));
    if (repoPtr == nullptr) {
        // GetSystemViewOverallRepo中已有日志报错
        return;
    }
    Dic::Protocol::SystemViewOverallReqParam requestParams;
    requestParams.rankId = "1";
    requestParams.deviceId = "1";
    SystemViewOverallHelper computeHelper;
    bool result = repoPtr->QueryDataForComputingOverallMetric(requestParams, computeHelper, database);
    EXPECT_EQ(result, true);
    EXPECT_EQ(computeHelper.cpuCubeOps.size(), 0);
    EXPECT_EQ(computeHelper.kernelEvents.size(), 0);
    EXPECT_EQ(computeHelper.bwdTrackId, 0);
}

TEST_F(SystemViewOverallTextRepoTest, QueryCommunicationOverlapOverallInfosTestWhenSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    auto repoPtr = SystemViewOverallRepoFactory::Instance()->GetSystemViewOverallRepo(
        DataBaseManager::Instance().GetDataType(database->GetDbPath()));
    if (repoPtr == nullptr) {
        // GetSystemViewOverallRepo中已有日志报错
        return;
    }
    Dic::Protocol::SystemViewOverallReqParam requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    SystemViewOverallHelper computeHelper;
    computeHelper.e2eTime = 136581.01; // 136581.01
    std::vector<Protocol::SystemViewOverallRes> responseBody;
    repoPtr->QueryCommunicationOverlapOverallInfos(requestParams, computeHelper, responseBody, database);
    EXPECT_EQ(responseBody.size(), 1);
    EXPECT_DOUBLE_EQ(responseBody[0].totalTime, 101963.203); // 101963.203
    EXPECT_DOUBLE_EQ(responseBody[0].ratio, 74.65); // 74.65
    EXPECT_EQ(responseBody[0].children.size(), 1); // Group 0 Communication
    EXPECT_DOUBLE_EQ(responseBody[0].children[0].totalTime, 101963.2); // 101963.2
    EXPECT_EQ(responseBody[0].children[0].children.size(), 2); // 2 for "Wait Time" and "Transmit Time"
    EXPECT_DOUBLE_EQ(responseBody[0].children[0].children[0].totalTime, 0); // 0 for Wait Time
    EXPECT_DOUBLE_EQ(responseBody[0].children[0].children[1].totalTime, 101963.2); // 101963.2 for Transmit Time
}

TEST_F(SystemViewOverallTextRepoTest, QueryCommunicationOpsTimeDataByGroupNameTestWhenSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    auto repoPtr = SystemViewOverallRepoFactory::Instance()->GetSystemViewOverallRepo(
        DataBaseManager::Instance().GetDataType(database->GetDbPath()));
    if (repoPtr == nullptr) {
        // GetSystemViewOverallRepo中已有日志报错
        return;
    }
    Dic::Protocol::SystemViewOverallReqParam requestParams;
    requestParams.rankId = "0";
    SystemViewOverallHelper computeHelper;
    UnitThreadsOperatorsResponse response;
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string sql = DataBaseManager::Instance().GetDataType(database->GetDbPath()) == DataType::TEXT
        ? TextSqlConstant::GetOverlapAnalysisTextSqlByType(requestParams)
        : TraceDatabaseSqlConst::GetOverlapAnalysisDbSqlByType(requestParams);
    std::string type = DataBaseManager::Instance().GetDataType(database->GetDbPath()) == DataType::TEXT
        ? "Communication(Not Overlapped)"
        : "2";
    uint64_t totalTime = 0;
    std::vector<Protocol::ThreadTraces> notOverlapData{};
    std::string deviceId = Dic::Module::Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId("0");
    requestParams.deviceId = deviceId;
    int intDeviceId = StringUtil::StringToInt(deviceId);
    ParamsForOAData paramsForOaData = {sql, type, minTimestamp};
    bool result = database->QueryOverlapAnalysisData(paramsForOaData, intDeviceId, notOverlapData, totalTime);
    EXPECT_TRUE(result);
    requestParams.categoryList = {"", "Group 0 Communication"};
    repoPtr->QueryCommunicationOpsTimeDataByGroupName(
        requestParams, minTimestamp, notOverlapData, response.body.sameOperatorsDetails, database);
    EXPECT_EQ(response.body.sameOperatorsDetails.size(), 4); // 4
    int index = 0;
    EXPECT_EQ(response.body.sameOperatorsDetails[index++].name, "hcom_allReduce__586_0_1");
    EXPECT_EQ(response.body.sameOperatorsDetails[index++].name, "hcom_allReduce__586_1_1");
    EXPECT_EQ(response.body.sameOperatorsDetails[index++].name, "hcom_allReduce__586_2_1");
    EXPECT_EQ(response.body.sameOperatorsDetails[index++].name, "hcom_allReduce__586_3_1");
}
}
