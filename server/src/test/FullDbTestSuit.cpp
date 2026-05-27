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
#include "TestSuit.h"
#include "DataBaseManager.h"
#include "ParamsParser.h"
#include "FullDbParser.h"
#include "ParserStatusManager.h"
#include "ProjectParserFactory.h"
#include "WsSessionManager.h"
#include "WsSessionImpl.h"
#include "RepositoryFactory.h"
#include "DataEngine.h"
#include "RenderEngine.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;
using namespace Dic::Module;
using namespace Dic;

class FullDbTestSuit : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        FullDb::FullDbParser::Instance().Reset();
        std::string currPath = Dic::FileUtil::GetCurrPath();
        const ParamsOption &option = ParamsParser::Instance().GetOption();
        ServerLog::Initialize(option.logPath, option.logSize, option.logLevel, to_string(option.wsPort));
        auto respotoryFactory = RepositoryFactory::Instance();
        auto dataEngine = DataEngine::Instance();
        dataEngine->SetRepositoryFactory(respotoryFactory);
        auto renderEngine = RenderEngine::Instance();
        renderEngine->SetDataEngineInterface(dataEngine);
        std::string dbPath3 = TestSuit::GetTestDataFile("full_db", "msprof_0.db");
        DataBaseManager::Instance().SetDataType(DataType::DB, dbPath3);
        DataBaseManager::Instance().SetFileType(FileType::MS_PROF, dbPath3);
        std::pair<std::string, ParserType> parserType = std::make_pair(dbPath3, ParserType::DB);
        ParserType allocType = parserType.second;
        std::shared_ptr<ProjectParserBase> factory = ParserFactory::GetProjectParser(allocType);
        ProjectTypeEnum projectType = factory->GetProjectType(dbPath3);
        std::vector<Global::ProjectExplorerInfo> projectExplorerInfoList;

        std::string warn;
        std::vector<std::string> parseFileList = factory->GetParseFileByImportFile(dbPath3, warn);
        Global::ProjectExplorerInfo projectExplorerInfo;
        projectExplorerInfo.fileName = dbPath3;
        projectExplorerInfo.projectName = dbPath3;
        projectExplorerInfo.projectType = static_cast<int64_t>(projectType);
        projectExplorerInfo.importType = "import";
        auto parseFileInfo = std::make_shared<ParseFileInfo>();
        parseFileInfo->parseFilePath = dbPath3;
        projectExplorerInfo.subParseFileInfo.push_back(parseFileInfo);
        projectExplorerInfoList.push_back(projectExplorerInfo);

        ImportActionRequest request;
        // 从ImportActionHandler可以看出，Parser方法的第一个参数是vector，但永远只有一个元素，所以DT中不要传多个元素的vector
        ImportActionResponse response;
        factory->Parser(projectExplorerInfoList, request, response);
        Timeline::DataBaseManager::Instance().SetRankIdFileIdMapping("2", dbPath3);
        Timeline::DataBaseManager::Instance().SetDbPathMapping("FullDb", dbPath3, "");
        Timeline::DataBaseManager::Instance().SetDbPathMapping("2", dbPath3, "");
        while (ParserStatusManager::Instance().GetParserStatus("2") != ParserStatus::FINISH_ALL) {
        }
    }

    static void TearDownTestSuite() { FullDbParser::Instance().Reset(); }
};
