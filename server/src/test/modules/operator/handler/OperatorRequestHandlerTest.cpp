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
#include <WsSessionManager.h>
#include "BaselineManager.h"
#include "BaselineManagerService.h"
#include "DataBaseManager.h"
#include "DbSummaryDataBase.h"
#include "FileUtil.h"
#include "ModuleRequestHandler.h"
#include "OperatorErrorManager.h"
#include "OperatorRequestHandler.h"
#include "OperatorProtocolResponse.h"
#include "QueryOpCategoryInfoHandler.h"
#include "QueryOpComputeUnitHandler.h"
#include "QueryOpStatisticInfoHandler.h"
#include "QueryOpDetailInfoHandler.h"
#include "QueryOpMoreInfoHandler.h"
#include "ExportOpDetailsHandler.h"
#include "ParamsParser.h"
#include "ProjectExplorerManager.h"
#include "WsSessionImpl.h"
#include "RenderEngine.h"
#include "../OperatorLogTestUtil.h"
#include "../../../TestSuit.h"

using namespace Dic::Server;
using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;
using namespace Dic::Module::Global;
using namespace Dic::Module::Operator;
using namespace Dic::Module::FullDb;

class OperatorRequestHandlerTest : public TestSuit {
  public:
    static void SetUpTestSuite() {
        capturingSession = std::make_shared<OperatorLogTestUtil::CapturingWsSession>();
        WsSessionManager::Instance().RemoveSession();
        WsSessionManager::Instance().AddSession(capturingSession);
    }
    static void TearDownTestSuite() {
        WsSessionManager::Instance().RemoveSession();
        capturingSession.reset();
        DataBaseManager::Instance().Clear();
        BaselineManagerService::ResetBaseline(true);
    }

    void SetUp() override {
        capturingSession->Reset();
        Dic::Module::ModuleRequestHandler::ResetRequestContextError();
        RemoveEmptyBaselineDb();
    }

    void TearDown() override {
        BaselineManager::Instance().Reset();
        DataBaseManager::Instance().Clear();
        RemoveEmptyBaselineDb();
        Dic::Module::ModuleRequestHandler::ResetRequestContextError();
        capturingSession->Reset();
    }

    static std::string GetEmptyBaselineDbPath() {
        return FileUtil::SplicePath(FileUtil::GetCurrPath(), "empty-baseline-db-244.db");
    }

    static void RemoveEmptyBaselineDb() {
        const auto path = GetEmptyBaselineDbPath();
        if (FileUtil::CheckFilePathExist(path)) {
            EXPECT_TRUE(FileUtil::RemoveFile(path));
        }
    }

    static void InitDbManager() {
        DataBaseManager::Instance().Clear();
        const ParamsOption &option = ParamsParser::Instance().GetOption();
        ServerLog::Initialize(option.logPath, option.logSize, option.logLevel, to_string(option.wsPort));
        std::string fullDbPath = Dic::FileUtil::SplicePath(testDataDir, "full_db", "msprof_0.db");
        DataBaseManager::Instance().SetDataType(DataType::DB, fullDbPath);
        auto summeryDatabase =
            std::dynamic_pointer_cast<DbSummaryDataBase, Dic::Module::Summary::VirtualSummaryDataBase>(
                DataBaseManager::Instance().CreateSummaryDatabase("2", fullDbPath));
        summeryDatabase->OpenDb(fullDbPath, false);
        auto renderEngine = GetRenderEngine();
        ASSERT_TRUE(renderEngine != nullptr);
        DataBaseManager::Instance().UpdateRankIdToDeviceId(fullDbPath, "2", "2");
    }

    static void InitBaseLineManager() {
        ProjectExplorerManager::Instance().InitSystemMemoryDbPath(testDataDir);
        InitProjectExplorerData();
    }

    static bool SetBaseLineManager() {
        InitBaseLineManager();
        // 创建DB场景的baseline基线manager
        std::string filePathText = Dic::FileUtil::SplicePath(testDataDir, "test_rank_0", "ASCEND_PROFILER_OUTPUT");
        BaselineInfo baselineInfo;
        baselineInfo.parsedFilePath = filePathText;
        BaselineSettingRequest request;
        request.projectName = "testProject";
        request.params.projectName = "testProject";
        request.params.filePath = filePathText;
        request.params.currentClusterPath = COMPARE;
        bool result = BaselineManagerService::InitBaselineData(request, baselineInfo);
        std::string notFinishTask = "";
        int index = 0;
        while (index < retry && !Dic::Module::Timeline::ParserStatusManager::Instance().IsAllFinished(notFinishTask)) {
            const int sleepTime = 2000;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
            index++;
        }
        return result;
    }

    static void SetBaselineIdOnly(const std::string &baselineId) {
        BaselineInfo info;
        info.rankId = baselineId;
        BaselineManager::Instance().SetBaselineInfo(info);
    }

    static void InitBaselineWithoutDevice(const std::string &baselineId) {
        InitDbManager();
        const std::string dbPath = TestSuit::GetTestDataFile("full_db", "msprof_0.db");
        auto database = std::dynamic_pointer_cast<DbSummaryDataBase, Dic::Module::Summary::VirtualSummaryDataBase>(
            DataBaseManager::Instance().CreateSummaryDatabase(baselineId, dbPath));
        ASSERT_NE(database, nullptr);
        ASSERT_TRUE(database->IsOpen() || database->AttachDb(dbPath));
        SetBaselineIdOnly(baselineId);
    }

    static std::unique_ptr<Dic::Protocol::OperatorDetailInfoRequest> MakeDetailRequest(
        const std::string &rankId = "2") {
        auto request = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
        request->params = {true, rankId, "", "Operator", -1, 1, 10, "duration", "descend"};
        return request;
    }

    template <typename ResponseType>
    static void ExpectFailureResponse(const ResponseType *response, Dic::Module::Operator::ErrorCode expectedError) {
        ASSERT_NE(response, nullptr);
        EXPECT_FALSE(response->result);
        ASSERT_TRUE(response->error.has_value());
        EXPECT_EQ(response->error->code, static_cast<int>(expectedError));
    }

    static void ClearProjectExplorerData() {
        ProjectExplorerManager::Instance().DeleteProjectAndFilePath("testProject", std::vector<std::string>());
        ProjectExplorerManager::Instance().DeleteProjectAndFilePath("testProjectDb", std::vector<std::string>());
    }

  protected:
    inline static std::shared_ptr<OperatorLogTestUtil::CapturingWsSession> capturingSession;
    inline static std::string testDataDir = TestSuit::GetTestDataFile();
    inline static int retry = 2;
    static ProjectExplorerInfo CreateProjectData(const std::string &projectName, const std::string &fileName,
        const std::string &importType, Dic::ProjectTypeEnum projectType, const std::vector<std::string> parseFileList) {
        ProjectExplorerInfo info;
        info.projectName = projectName;
        info.fileName = fileName;
        info.importType = importType;
        info.projectType = static_cast<int64_t>(projectType);
        for (const auto &item : parseFileList) {
            auto parseFileInfo = std::make_shared<ParseFileInfo>();
            parseFileInfo->parseFilePath = item;
            parseFileInfo->type = ParseFileType::RANK;
            parseFileInfo->subId = item;
            info.AddSubParseFileInfo(parseFileInfo);
        }
        return info;
    }

    static void InitProjectExplorerData() {
        std::string filePathText = Dic::FileUtil::SplicePath(testDataDir, "test_rank_0", "ASCEND_PROFILER_OUTPUT");
        std::string filePathDb = Dic::FileUtil::SplicePath(testDataDir, "full_db", "ascend_pytorch_profiler.db");
        std::vector<ProjectExplorerInfo> infos;
        std::vector<std::string> parseFileList{filePathText};
        ProjectExplorerInfo info = CreateProjectData(
            "testProject", "projectFilePath", "import", Dic::ProjectTypeEnum::TEXT_CLUSTER, parseFileList);
        infos.push_back(info);
        std::for_each(infos.begin(), infos.end(),
            [](const auto &item) { ProjectExplorerManager::Instance().SaveProjectExplorer(item, false); });

        std::vector<ProjectExplorerInfo> dbInfos;
        std::vector<std::string> parseDbFileList{filePathDb};
        ProjectExplorerInfo dbInfo = CreateProjectData(
            "testProjectDb", "projectFilePathDb", "import", Dic::ProjectTypeEnum::DB, parseDbFileList);
        dbInfos.push_back(dbInfo);
        std::for_each(dbInfos.begin(), dbInfos.end(),
            [](const auto &item) { ProjectExplorerManager::Instance().SaveProjectExplorer(item, false); });
    }
};

TEST_F(OperatorRequestHandlerTest, GroupLogContextsPreserveKnownAndEmptyValuesAndHideUnknownValues) {
    class ContextHandler : public OperatorRequestHandler {
      public:
        using OperatorRequestHandler::GetLogContext;
        using OperatorRequestHandler::GetBaselineLogContext;
    };
    const std::vector<std::pair<std::string, std::string>> groups = {{"Operator", "Operator"},
        {"Operator Type", "Operator Type"}, {"Input Shape", "Input Shape"},
        {"Communication Operator", "Communication Operator"},
        {"Communication Operator Type", "Communication Operator Type"}, {"", ""}, {"Unsupported Group", "unknown"},
        {std::string(1024 * 1024, 'x'), "unknown"}};
    for (const auto &[group, display] : groups) {
        SCOPED_TRACE(::testing::Message() << "group size=" << group.size() << ", display=" << display);
        auto request = MakeDetailRequest("244");
        request->params.group = group;
        const std::string context = "rankId=244, group=" + display;
        EXPECT_TRUE(ContextHandler::GetLogContext(request->params.rankId, request->params.group) == context);
        for (bool isCompare : {false, true}) {
            request->params.isCompare = isCompare;
            const std::string compareContext = context + ", isCompare=" + (isCompare ? "true" : "false");
            EXPECT_TRUE(ContextHandler::GetLogContext(request->params.rankId, request->params.group,
                            request->params.isCompare) == compareContext);
            EXPECT_EQ(request->params.isCompare, isCompare);
        }
        EXPECT_TRUE(ContextHandler::GetBaselineLogContext(request->params.rankId, "baseline-244",
                        request->params.group) == "rankId=244, baselineName=baseline-244, group=" + display);
        EXPECT_TRUE(request->params.group == group);
        EXPECT_EQ(request->params.rankId, "244");
    }
}

TEST_F(OperatorRequestHandlerTest, DetailUnknownAndLongGroupsLogUnknownWithoutChangingResourceOutcomes) {
    DataBaseManager::Instance().Clear();
    QueryOpDetailInfoHandler handler;
    for (const auto &group : {std::string("Unsupported Group"), std::string(1024 * 1024, 'x')}) {
        for (bool isCompare : {false, true}) {
            SCOPED_TRACE(::testing::Message() << "group size=" << group.size() << ", isCompare=" << isCompare);
            capturingSession->Reset();
            Dic::Module::ModuleRequestHandler::ResetRequestContextError();
            auto request = MakeDetailRequest("244");
            request->params.group = group;
            request->params.isCompare = isCompare;
            const auto mark = OperatorLogTestUtil::Mark();
            EXPECT_EQ(handler.HandleRequest(std::move(request)), !isCompare);
            const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorDetailInfoResponse>();
            ASSERT_NE(response, nullptr);
            if (isCompare) {
                ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::GET_DEVICE_ID_FAILED);
            } else {
                EXPECT_TRUE(response->result);
                ASSERT_TRUE(response->error.has_value());
                EXPECT_EQ(response->error->code, Dic::UNKNOW_ERROR);
                EXPECT_TRUE(response->error->message.empty());
            }
            EXPECT_TRUE(response->data.empty());
            EXPECT_EQ(response->total, 0);
            OperatorLogTestUtil::ExpectSingleOperatorLog(mark, isCompare ? 0 : 1, isCompare ? 1 : 0, "QueryDetail",
                isCompare ? "ResolveDevice" : "GetDatabase",
                {"rankId=244, group=unknown, isCompare=" + std::string(isCompare ? "true" : "false"),
                    isCompare ? "cause=no device mapping was found" : "cause=no summary database was found",
                    "suggestion="},
                {"group=" + group.substr(0, 32)});
        }
    }
}

TEST_F(OperatorRequestHandlerTest, CategoryMissingDeviceMappingLogsSingleError) {
    DataBaseManager::Instance().Clear();
    const std::string dbPath = FileUtil::SplicePath(testDataDir, "full_db", "msprof_0.db");
    DataBaseManager::Instance().SetDataType(DataType::DB, dbPath);
    ASSERT_NE(DataBaseManager::Instance().CreateSummaryDatabase("244", dbPath), nullptr);
    QueryOpCategoryInfoHandler handler;
    auto request = std::make_unique<Dic::Protocol::OperatorCategoryInfoRequest>();
    request->params.rankId = "244";
    request->params.group = "Operator";
    request->params.topK = -1;
    const auto mark = OperatorLogTestUtil::Mark();
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorCategoryInfoResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_FALSE(response->result);
    ASSERT_TRUE(response->error.has_value());
    EXPECT_EQ(response->error->code, static_cast<int>(Dic::Module::Operator::ErrorCode::GET_DEVICE_ID_FAILED));
    EXPECT_TRUE(response->data.empty());
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryCategory", "ResolveDevice",
        {"rankId=244", "group=Operator", "cause=no device mapping was found", "suggestion="},
        {"Don't find deviceId in rankIdToDeviceIdMap"});
}

TEST_F(OperatorRequestHandlerTest, DetailMissingBaselineDatabaseLogsBasenameOnlyWarn) {
    InitDbManager();
    const std::string baselineId = FileUtil::SplicePath(FileUtil::GetCurrPath(), "missing-baseline-db-244");
    SetBaselineIdOnly(baselineId);
    QueryOpDetailInfoHandler handler;
    auto request = MakeDetailRequest();
    const auto mark = OperatorLogTestUtil::Mark();
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorDetailInfoResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryDetail", "GetBaselineDatabase",
        {"rankId=2", "baselineName=missing-baseline-db-244", "group=Operator",
            "cause=no baseline summary database was found", "suggestion="},
        {FileUtil::GetCurrPath()});
}

TEST_F(OperatorRequestHandlerTest, DetailMissingCurrentDatabaseAndDeviceLogsSingleError) {
    DataBaseManager::Instance().Clear();
    QueryOpDetailInfoHandler handler;
    auto request = MakeDetailRequest("244");
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));

    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorDetailInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::GET_DEVICE_ID_FAILED);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryDetail", "ResolveDevice",
        {"rankId=244", "group=Operator", "isCompare=true", "cause=no device mapping was found", "suggestion="},
        {"Operator database is unavailable"});
}

TEST_F(OperatorRequestHandlerTest, DetailMissingBaselineDeviceReturnsError) {
    const std::string baselineId = "baseline-without-device-244";
    InitBaselineWithoutDevice(baselineId);
    QueryOpDetailInfoHandler handler;
    auto request = MakeDetailRequest();
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));

    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorDetailInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::GET_DEVICE_ID_FAILED);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryDetail", "ResolveBaselineDevice",
        {"rankId=2", "baselineName=baseline-without-device-244", "group=Operator",
            "cause=no baseline device mapping was found", "suggestion="},
        {});
}

TEST_F(OperatorRequestHandlerTest, StatisticMissingBaselineDeviceReturnsError) {
    const std::string baselineId = "statistic-baseline-without-device-244";
    InitBaselineWithoutDevice(baselineId);
    QueryOpStatisticInfoHandler handler;
    auto request = std::make_unique<Dic::Protocol::OperatorStatisticInfoRequest>();
    request->params.rankId = "2";
    request->params.group = "Operator Type";
    request->params.topK = -1;
    request->params.isCompare = true;
    request->params.pageSize = 10;
    request->params.current = 1;
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));

    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorStatisticInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::GET_DEVICE_ID_FAILED);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryStatistic", "ResolveBaselineDevice",
        {"rankId=2", "baselineName=statistic-baseline-without-device-244", "group=Operator Type",
            "cause=no baseline device mapping was found", "suggestion="},
        {});
}

TEST_F(OperatorRequestHandlerTest, DetailBaselineDatabaseFailureUsesBaselineContext) {
    InitDbManager();
    const std::string baselineId = "empty-baseline-db-244";
    const std::string baselinePath = GetEmptyBaselineDbPath();
    DataBaseManager::Instance().SetDataType(DataType::DB, baselinePath);
    auto baselineDatabase = std::dynamic_pointer_cast<DbSummaryDataBase, Dic::Module::Summary::VirtualSummaryDataBase>(
        DataBaseManager::Instance().CreateSummaryDatabase(baselineId, baselinePath));
    ASSERT_NE(baselineDatabase, nullptr);
    ASSERT_TRUE(baselineDatabase->CreateDbIfNotExist(baselinePath));
    ASSERT_TRUE(baselineDatabase->IsOpen() || baselineDatabase->AttachDb(baselinePath));
    DataBaseManager::Instance().UpdateRankIdToDeviceId(baselinePath, baselineId, "0");
    SetBaselineIdOnly(baselineId);
    QueryOpDetailInfoHandler handler;
    auto request = MakeDetailRequest();
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));

    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorDetailInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::QUERY_ALL_DETAIL_FAILED);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryDetail", "PrepareSql",
        {"rankId=2", "deviceId=0", "group=Operator", "baselineName=empty-baseline-db-244", "cause=", "suggestion="},
        {baselinePath});
}

TEST_F(OperatorRequestHandlerTest, MoreInfoMissingDatabaseLogsSingleError) {
    DataBaseManager::Instance().Clear();
    const std::string rankId = "245";
    const std::string fileId = "missing-operator-db-245";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);
    DataBaseManager::Instance().UpdateRankIdToDeviceId(fileId, rankId, "0");
    QueryOpMoreInfoHandler handler;
    auto request = std::make_unique<Dic::Protocol::OperatorMoreInfoRequest>();
    request->params.rankId = rankId;
    request->params.group = "Communication Operator Type";
    request->params.opType = "Mat%Mul";
    request->params.accCore = "HCCL";
    const auto mark = OperatorLogTestUtil::Mark();
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorMoreInfoResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_FALSE(response->result);
    ASSERT_TRUE(response->error.has_value());
    EXPECT_EQ(response->error->code, static_cast<int>(Dic::Module::Operator::ErrorCode::QUERY_MORE_INFO_FAILED));
    EXPECT_EQ(response->total, 0);
    EXPECT_TRUE(response->data.empty());
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryMoreInfo", "GetDatabase",
        {"rankId=245", "group=Communication Operator Type", "cause=no summary database was found", "suggestion="},
        {"opType=", "accCore=", "Can't find summary database"});
}

TEST_F(OperatorRequestHandlerTest, MoreInfoMissingOperatorIdentityLogsSingleValidationWarn) {
    QueryOpMoreInfoHandler handler;
    auto request = std::make_unique<Dic::Protocol::OperatorMoreInfoRequest>();
    request->params.rankId = "244";
    request->params.group = "Operator Type";
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));

    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorMoreInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::PARAMS_ERROR);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryMoreInfo", "ValidateRequest",
        {"cause=opName and opType are invalid. Parameter is empty.", "suggestion="},
        {"rankId=", "deviceId=", "group=", "opName=", "opType="});
}

TEST_F(OperatorRequestHandlerTest, MoreInfoMissingDatabaseAndDeviceLogsDeviceError) {
    DataBaseManager::Instance().Clear();
    QueryOpMoreInfoHandler handler;
    auto request = std::make_unique<Dic::Protocol::OperatorMoreInfoRequest>();
    request->params.rankId = "246";
    request->params.group = "Input Shape";
    request->params.opName = "MatMul";
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));

    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorMoreInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::GET_DEVICE_ID_FAILED);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryMoreInfo", "ResolveDevice",
        {"rankId=246", "group=Input Shape", "cause=no device mapping was found", "suggestion="},
        {"opName=", "inputShape=", "accCore=", "Operator database is unavailable"});
}

TEST_F(OperatorRequestHandlerTest, MoreInfoOptionalTableStatesReturnConsistentResults) {
    DataBaseManager::Instance().Clear();
    const std::string rankId = "247";
    const std::string dbPath = GetEmptyBaselineDbPath();
    DataBaseManager::Instance().SetDataType(DataType::DB, dbPath);
    auto database = std::dynamic_pointer_cast<DbSummaryDataBase, Dic::Module::Summary::VirtualSummaryDataBase>(
        DataBaseManager::Instance().CreateSummaryDatabase(rankId, dbPath));
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(database->CreateDbIfNotExist(dbPath));
    ASSERT_TRUE(database->IsOpen() || database->AttachDb(dbPath));
    DataBaseManager::Instance().UpdateRankIdToDeviceId(dbPath, rankId, "0");
    const auto makeRequest = [&rankId]() {
        auto request = std::make_unique<Dic::Protocol::OperatorMoreInfoRequest>();
        request->params = {
            rankId, "", "Communication Operator Type", 15, "AllReduce", "", "", "HCCL", 1, 10, "", "", {}};
        return request;
    };

    QueryOpMoreInfoHandler handler;
    auto mark = OperatorLogTestUtil::Mark();
    EXPECT_TRUE(handler.HandleRequest(makeRequest()));
    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorMoreInfoResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->total, 0);
    EXPECT_TRUE(response->data.empty());
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryMoreInfo", "CheckTable",
        {"rankId=247", "deviceId=0", "group=Communication Operator Type", "table=COMMUNICATION_OP",
            "cause=the communication table is missing", "suggestion="},
        {});

    ASSERT_TRUE(database->ExecSql(
        "CREATE TABLE COMMUNICATION_OP (opType INTEGER, opName INTEGER, startNs INTEGER, endNs INTEGER, "
        "waitNs INTEGER, connectionId INTEGER);"
        "CREATE TABLE TASK (deviceId INTEGER, connectionId INTEGER);"
        "CREATE TABLE STRING_IDS (id INTEGER, value TEXT);"));
    database->CloseDb();
    ASSERT_TRUE(database->OpenDb(dbPath, false));
    capturingSession->Reset();
    Dic::Module::ModuleRequestHandler::ResetRequestContextError();
    mark = OperatorLogTestUtil::Mark();
    EXPECT_TRUE(handler.HandleRequest(makeRequest()));
    response = capturingSession->GetResponse<Dic::Protocol::OperatorMoreInfoResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->total, 0);
    EXPECT_TRUE(response->data.empty());
    EXPECT_EQ(OperatorLogTestUtil::Count(OperatorLogTestUtil::ReadSince(mark), "[Operator]"), 0);

    capturingSession->Reset();
    Dic::Module::ModuleRequestHandler::ResetRequestContextError();
    database->CloseDb();
    mark = OperatorLogTestUtil::Mark();
    EXPECT_FALSE(handler.HandleRequest(makeRequest()));
    response = capturingSession->GetResponse<Dic::Protocol::OperatorMoreInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::QUERY_MORE_INFO_FAILED);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryMoreInfo", "CheckTable",
        {"rankId=247", "deviceId=0", "group=Communication Operator Type", "sqliteCode=" + std::to_string(SQLITE_MISUSE),
            "cause=database is closed", "suggestion="},
        {"Optional data is unavailable"});
}

TEST_F(OperatorRequestHandlerTest, QueryOpCategoryInfoHandlerNormalTest) {
    Dic::Module::Operator::QueryOpCategoryInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorCategoryInfoRequest>();
    requestPtr->params.rankId = "0";
    requestPtr->params.group = "Operator";
    requestPtr->params.topK = -1;
    requestPtr->fileId = DataBaseManager::Instance().GetFileIdByRankId("0");
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, QueryOpCategoryInfoDoesNotLogOverlongRankIdValue) {
    Dic::Module::Operator::QueryOpCategoryInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorCategoryInfoRequest>();
    requestPtr->params.rankId = "";
    requestPtr->params.group = "Operator";
    requestPtr->params.topK = -1;
    const std::string invalidRankId = std::string(503, 'r') + "\xE4\xB8\xAD" + std::string(100, 'r');
    requestPtr.get()->params.rankId = invalidRankId;
    requestPtr->fileId = "";
    const auto mark = OperatorLogTestUtil::Mark();
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorCategoryInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::PARAMS_ERROR);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryCategory", "ValidateRequest",
        {"cause=rankId is invalid.", "suggestion="},
        {"rankId=", "rankIdLength=", "group=", "[truncated]", invalidRankId});
}

TEST_F(OperatorRequestHandlerTest, QueryOpComputeUnitHandlerNormalTest) {
    Dic::Module::Operator::QueryOpComputeUnitHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorComputeUnitInfoRequest>();
    requestPtr->params.rankId = "0";
    requestPtr->fileId = DataBaseManager::Instance().GetFileIdByRankId("0");
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, ComputeUnitInvalidRequestLogsSingleWarn) {
    Dic::Module::Operator::QueryOpComputeUnitHandler handler;
    auto request = std::make_unique<Dic::Protocol::OperatorComputeUnitInfoRequest>();
    request->params.rankId = "244";
    request->params.group = "Operator";
    request->params.topK = -2;
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));

    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorComputeUnitInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::PARAMS_ERROR);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryComputeUnit", "ValidateRequest",
        {"cause=topK must be greater than or equal to -1.", "suggestion="}, {"rankId=", "group="});
}

TEST_F(OperatorRequestHandlerTest, StatisticInvalidRequestLogsSingleWarn) {
    Dic::Module::Operator::QueryOpStatisticInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorStatisticInfoRequest>();
    const auto mark = OperatorLogTestUtil::Mark();
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorStatisticInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::PARAMS_ERROR);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryStatistic", "ValidateRequest",
        {"cause=topK must not be zero.", "suggestion="}, {"rankId=", "group=", "isCompare="});
}

TEST_F(OperatorRequestHandlerTest, QueryOpStatisticInfoHandlerCmplTest) {
    Dic::Module::Operator::QueryOpStatisticInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorStatisticInfoRequest>();
    requestPtr->params.rankId = "0";
    requestPtr->params.group = "Operator Type";
    requestPtr->params.topK = -1;
    requestPtr->params.isCompare = true;
    // 10 表示分页最小是10条
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = 1;
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerNormalTest) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerNormal2Test) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    const uint64_t ten = 10;
    const uint64_t oneOneOne = 100;
    const uint64_t one = 1;
    requestPtr->params.topK = ten;
    requestPtr->params.pageSize = oneOneOne;
    requestPtr->params.current = one;
    requestPtr->params.rankId = "1";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerNormal3Test) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    const uint64_t ten = 10;
    const uint64_t oneOneOne = 100;
    const uint64_t one = 1;
    requestPtr->params.topK = ten;
    requestPtr->params.pageSize = oneOneOne;
    requestPtr->params.current = one;
    requestPtr->params.rankId = "1";
    requestPtr->params.isCompare = true;
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, QueryOpMoreInfoHandlerReturnsFullDbData) {
    InitDbManager();
    QueryOpMoreInfoHandler handler;
    auto request = std::make_unique<Dic::Protocol::OperatorMoreInfoRequest>();
    request->params = {"2", "", "Operator Type", 15, "Cast", "", "", "AI_VECTOR_CORE", 1, 10, "", "", {}};

    EXPECT_TRUE(handler.HandleRequest(std::move(request)));

    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorMoreInfoResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->total, 2);
    EXPECT_EQ(response->data.size(), 2);
}

TEST_F(OperatorRequestHandlerTest, MoreInfoInvalidFilterLogsGenerateSqlOnce) {
    InitDbManager();
    QueryOpMoreInfoHandler handler;
    auto request = std::make_unique<Dic::Protocol::OperatorMoreInfoRequest>();
    request->params = {
        "2", "", "Operator Type", 15, "Cast", "", "", "AI_VECTOR_CORE", 1, 10, "", "", {{"name", "invalid value"}}};
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));

    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorMoreInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::QUERY_MORE_INFO_FAILED);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryMoreInfo", "GenerateSql",
        {"cause=a filter failed SQL validation", "suggestion="},
        {"rankId=", "group=", "opType=", "invalid value", "[PrepareSql]"});
}

TEST_F(OperatorRequestHandlerTest, QueryOpStatisticInfoHandlerSuccessWhenBaselineIsDbGroupByOperatorType) {
    InitDbManager();
    Dic::Module::Operator::QueryOpStatisticInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorStatisticInfoRequest>();
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Operator Type";
    requestPtr->params.topK = -1; // -1 表示topK取全部数据
    requestPtr->params.isCompare = true;
    // 10 表示分页最小是10条
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = 1;
    requestPtr->fileId = DataBaseManager::Instance().GetFileIdByRankId("2");
    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, QueryOpStatisticInfoHandlerSuccessAndOrderByOpTypeAndTopKIs15GroupByInputShape) {
    InitDbManager();
    Dic::Module::Operator::QueryOpStatisticInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorStatisticInfoRequest>();
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Input Shape";
    requestPtr->params.topK = 15; // 15表示topK取15条数据
    requestPtr->params.isCompare = true;
    requestPtr->params.orderBy = "opType";
    // 10 表示分页最小是10条
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = 1;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, QueryOpStatisticInfoHandlerSuccessWhenBaselineIsDbGroupByHCCLOperatorType) {
    InitDbManager();
    Dic::Module::Operator::QueryOpStatisticInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorStatisticInfoRequest>();
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Communication Operator Type";
    // topK给一个极大值
    requestPtr->params.topK = 10000000; // 10000000表示topK是一个极大值
    requestPtr->params.isCompare = true;
    requestPtr->params.orderBy = "count";
    requestPtr->params.order = "ascend";
    // 10 表示分页最小是10条
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = 1;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, QueryOpStatisticInfoHandlerSuccessGroupByHCCLOperatorTypeOrderDesc) {
    InitDbManager();
    Dic::Module::Operator::QueryOpStatisticInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorStatisticInfoRequest>();
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Communication Operator Type";
    // topK给一个极大值
    requestPtr->params.topK = 10000000; // 10000000表示topK是一个极大值
    requestPtr->params.isCompare = true;
    requestPtr->params.orderBy = "count";
    requestPtr->params.order = "descend";
    // 10 表示分页最小是10条
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = 1;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

// QueryOpDetailInfoHandler 测试
TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerFailedWhenBsesLineIsNotSet) {
    InitDbManager();
    BaselineManager::Instance().Reset();
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = MakeDetailRequest();
    // topK给一个极大值
    requestPtr->params.topK = 10000000; // 10000000表示topK是一个极大值
    const auto mark = OperatorLogTestUtil::Mark();
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorDetailInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::GET_BASELINE_ID_FAILED);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryDetail", "ResolveBaseline",
        {"rankId=2", "group=Operator", "isCompare=true", "cause=baseline is not configured", "suggestion="}, {});
}

// ExportOpDetailsHandler 测试
TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerSuccessGroupByOperatorIsNotCompare) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Operator";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerSuccessGroupByOperatorTypeIsNotCompare) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Operator Type";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerSuccessGroupByInputShapeIsNotCompare) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Input Shape";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerSuccessGroupByCommunicationOperatorIsNotCompare) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Communication Operator";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerSuccessGroupByCommunicationOperatorTypeIsNotCompare) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Communication Operator Type";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerSuccessGroupByOperatorTypeIsCompare) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.isCompare = true;
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Operator Type";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerSuccessGroupByInputShapeIsCompare) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.isCompare = true;
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Input Shape";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerSuccessGroupByCommunicationOperatorIsCompare) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.isCompare = true;
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Communication Operator";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerSuccessGroupByCommunicationOperatorTypeIsCompare) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.isCompare = true;
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Communication Operator Type";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerFailTopKIsIllegal) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.isCompare = true;
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "Communication Operator Type";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MIN;

    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, ExportOpDetailsHandlerGroupByIsIllegal) {
    InitDbManager();
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    requestPtr->params.isCompare = true;
    requestPtr->params.rankId = "2";
    requestPtr->params.group = "UnKnow";
    // topK给一个极大值
    requestPtr->params.topK = INT64_MAX;

    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
    ClearProjectExplorerData();
}

TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerFailedWithabnormalTopK) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    requestPtr->params.group = "Operator";
    // abnormal topK -5
    requestPtr->params.topK = -5;
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerFailedWithabnormalPagesize) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    requestPtr->params.group = "Operator";
    // normal topK 10
    requestPtr->params.topK = 10;
    requestPtr->params.pageSize = MAX_PAGESIZE + 1;
    requestPtr->params.rankId = "2";
    requestPtr->params.current = 1;
    const auto mark = OperatorLogTestUtil::Mark();
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
    const auto *response = capturingSession->GetResponse<Dic::Protocol::OperatorDetailInfoResponse>();
    ExpectFailureResponse(response, Dic::Module::Operator::ErrorCode::PARAMS_ERROR);
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryDetail", "ValidateRequest",
        {"cause=pagesize:", "is invalid", "suggestion="}, {"rankId=", "group=", "pageSize="});
}

TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerFailedWithabnormalCurrent) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    requestPtr->params.group = "Operator";
    // normal topK 10
    requestPtr->params.topK = 10;
    // normal pageSize 10
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = MIN_CURRENT_PAGE;
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerFailedWithabnormalRankid) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    requestPtr->params.group = "Operator";
    // normal topK 10
    requestPtr->params.topK = 10;
    // normal pageSize 10
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = 1;
    requestPtr->params.rankId = "";
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerFailedWithabnormalOrder) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    requestPtr->params.group = "Operator";
    // normal topK 10
    requestPtr->params.topK = 10;
    // normal pageSize 10
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = 1;
    requestPtr->params.rankId = "2";
    requestPtr->params.orderBy = "";
    requestPtr->params.order = "";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(OperatorRequestHandlerTest, QueryOpDetailInfoHandlerFailedWithabnormalQuery) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    requestPtr->params.group = "Operator";
    // normal topK 10
    requestPtr->params.topK = 10;
    // normal pageSize 10
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = 1;
    requestPtr->params.rankId = "0";
    requestPtr->params.orderBy = "count";
    requestPtr->params.order = "descend";
    requestPtr->params.isCompare = false;
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
}
