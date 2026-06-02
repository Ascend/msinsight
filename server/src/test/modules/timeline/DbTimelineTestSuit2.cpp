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
#include "FullDbParser.h"
#include "ParamsParser.h"
#include "WsSessionManager.h"
#include "WsSessionImpl.h"
#include "ProjectParserFactory.h"
#include "DataBaseManager.h"
#include "DbTraceDataBase.h"
#include "TestSuit.h"

using namespace Dic::Module;
using namespace Dic::Module::FullDb;

class DbTimelineTestSuit2 : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        FullDb::FullDbParser::Instance().Reset();
        const ParamsOption &option = ParamsParser::Instance().GetOption();
        ServerLog::Initialize(option.logPath, option.logSize, option.logLevel, to_string(option.wsPort));
        std::string dbPath =
            FileUtil::SplicePath(TestSuit::GetRootTestPath(), "data", "pytorch", "db", "level2", "rank0_ascend_pt");

        DataBaseManager::Instance().SetDataType(DataType::DB, dbPath);
        std::pair<std::string, ParserType> parserType = std::make_pair(dbPath, ParserType::DB);
        ParserType allocType = parserType.second;
        std::shared_ptr<ProjectParserBase> factory = ParserFactory::GetProjectParser(allocType);
        ProjectTypeEnum projectType = factory->GetProjectType(dbPath);
        std::vector<Global::ProjectExplorerInfo> projectExplorerInfoList;

        std::string warn;
        std::vector<std::string> parseFileList = factory->GetParseFileByImportFile(dbPath, warn);
        Global::ProjectExplorerInfo projectExplorerInfo;
        projectExplorerInfo.fileName = dbPath;
        projectExplorerInfo.projectName = dbPath;
        projectExplorerInfo.projectType = static_cast<int64_t>(projectType);
        projectExplorerInfo.importType = "import";
        auto parseFileInfo = std::make_shared<ParseFileInfo>();
        parseFileInfo->parseFilePath = dbPath;
        projectExplorerInfo.subParseFileInfo.push_back(parseFileInfo);
        projectExplorerInfoList.push_back(projectExplorerInfo);

        if (allocType != ParserType::JSON) {
            ParserFactory::Reset();
        }
        ImportActionRequest request;
        ImportActionResponse response;
        // 从ImportActionHandler可以看出，Parser方法的第一个参数是vector，但永远只有一个元素，所以DT中不要传多个元素的vector
        factory->Parser(projectExplorerInfoList, request, response);

        Timeline::DataBaseManager::Instance().SetDbPathMapping("FullDbNew", dbPath, "");
        Timeline::DataBaseManager::Instance().SetDbPathMapping("2", dbPath, "");
        while (ParserStatusManager::Instance().GetParserStatus(cardId) != ParserStatus::FINISH_ALL) {
        }
    }

    static void TearDownTestSuite() { FullDbParser::Instance().Reset(); }

    static std::string cardId;
};

std::string DbTimelineTestSuit2::cardId = "ubuntu3538958389648580163_0 0";

TEST_F(DbTimelineTestSuit2, FullDb_of_QueryKernelDetailData_WithInvalidKey) {
    Dic::Protocol::KernelDetailsParams requestParams;
    requestParams.current = 1;
    requestParams.pageSize = 20; // pageSize = 20
    requestParams.order = "ASC";
    requestParams.orderBy = "name";
    requestParams.coreType = "HCCL";
    requestParams.searchName = "";
    requestParams.rankId = "2";
    requestParams.filters.emplace_back("#&$", "hcom");
    Dic::Protocol::KernelDetailsBody responseBody;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = std::dynamic_pointer_cast<DbTraceDataBase, Timeline::VirtualTraceDatabase>(
        Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(cardId));

    auto result = database->QueryKernelDetailData(requestParams, responseBody, minTimestamp);

    EXPECT_EQ(result, false);
}

TEST_F(DbTimelineTestSuit2, FullDb_of_QueryKernelDetailData_QueryHCCLType) {
    Dic::Protocol::KernelDetailsParams requestParams;
    requestParams.current = 1;
    requestParams.pageSize = 20; // pageSize = 20
    requestParams.order = "ASC";
    requestParams.orderBy = "name";
    requestParams.coreType = "HCCL";
    requestParams.searchName = "";
    requestParams.rankId = "2";
    requestParams.filters.emplace_back("type", "hcom");
    Dic::Protocol::KernelDetailsBody responseBody;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = std::dynamic_pointer_cast<DbTraceDataBase, Timeline::VirtualTraceDatabase>(
        Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(cardId));

    database->QueryKernelDetailData(requestParams, responseBody, minTimestamp);

    EXPECT_EQ(responseBody.kernelDetails.size(), 4); // size = 4
    EXPECT_EQ(responseBody.acceleratorCoreList.size(), 4);
}

TEST_F(DbTimelineTestSuit2, QueryByteAlignmentAnalyzerRawDataTest) {
    std::vector<ByteAlignmentAnalyzerLargeOperatorInfo> largeOpInfo;
    std::vector<ByteAlignmentAnalyzerSmallOperatorInfo> smallOpInfo;
    auto database = std::dynamic_pointer_cast<DbTraceDataBase, Timeline::VirtualTraceDatabase>(
        Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(cardId));
    bool result = database->QueryByteAlignmentAnalyzerRawData(largeOpInfo, smallOpInfo);
    ASSERT_TRUE(result);
    ASSERT_EQ(largeOpInfo.size(), 4); // 4
    ASSERT_EQ(smallOpInfo.size(), 72); // 72
}
