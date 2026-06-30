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

#include "WsSessionManager.h"
#include "ProjectExplorerManager.h"
#include "TimeUtil.h"
#include "ClusterFileParser.h"
#include "ProjectParserFactory.h"
#include "ProjectAnalyze.h"
#include "BaselineManagerService.h"
#include "ParserIE.h"
#include "ParserStatusManager.h"
#include "ImportActionHandler.h"

using namespace Dic;
using namespace Dic::Server;
using namespace Dic::Module::Global;
using namespace Dic::Module::Timeline;

bool ImportActionHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<ImportActionRequest &>(*requestPtr);
    ServerLog::Info("Import action request handler start");
    std::unique_ptr<ImportActionResponse> responsePtr = std::make_unique<ImportActionResponse>();
    ImportActionResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string warnMsg;
    if (!request.params.CommonCheck(warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    // 清理当前的基线缓存
    BaselineManagerService::ResetBaseline(true);
    ParserStatusManager::Instance().ResetParse();
    if (request.params.projectAction == ProjectActionEnum::ADD_FILE) {
        // ConvertToRealPath 调用 FileUtil::ConvertToRealPath 方法，其中 FileUtil::CheckDirValid 已做软链接检查
        if (!request.params.ConvertToRealPath(warnMsg)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
        if (!ImportFile(request, warnMsg)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    } else if (request.params.projectAction == ProjectActionEnum::TRANSFER_PROJECT) {
        if (!TransferProject(request)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    }
    return true;
}

void ImportActionHandler::SendParseFailEvent(const std::string &message) {
    auto event = std::make_unique<ParseFailEvent>();
    event->moduleName = MODULE_TIMELINE;
    event->result = false;
    event->body.error = message;
    SendEvent(std::move(event));
}

void ImportActionHandler::LogIfFileNotExist(const Global::ProjectExplorerInfo &projectExplorerInfo) {
    // 拖拽的文件原始文件不会保存在本地，因此不需要对拖拽的文件路径进行校验
    if (projectExplorerInfo.importType == "drag") {
        return;
    }
    if (!FileUtil::CheckFilePathExist(projectExplorerInfo.fileName)) {
        std::string message = "paths do not exist: " + projectExplorerInfo.fileName;
        ServerLog::Warn(message);
    }
}

bool ImportActionHandler::TransferProject(ImportActionRequest &request) {
    // 切换项目时，会对一级目录下所有内容进行加载
    std::vector<Global::ProjectExplorerInfo> projectExplorerInfo =
        Global::ProjectExplorerManager::Instance().QueryProjectExplorer(
            request.params.projectName, std::vector<std::string>());
    if (projectExplorerInfo.empty()) {
        ServerLog::Warn("params error, project explorer info is not existed.");
        SetTimelineError(ErrorCode::PROJECT_EXPLORER_NOT_EXISTED);
        return false;
    }
    std::for_each(projectExplorerInfo.begin(), projectExplorerInfo.end(),
        [](const auto &project) { ImportActionHandler::LogIfFileNotExist(project); });
    auto invalid = std::any_of(projectExplorerInfo.begin(), projectExplorerInfo.end(), [](const auto &project) {
        return (project.projectType < static_cast<int>(ProjectTypeEnum::DB) ||
            project.projectType > static_cast<int>(ProjectTypeEnum::OTHER));
    });
    if (invalid) {
        ServerLog::Warn("Project type invalid!");
        SetTimelineError(ErrorCode::PROJECT_TYPE_INVALID);
        return false;
    }
    auto response = std::make_unique<ImportActionResponse>();
    response->body.reset = IsNeedReset(request);
    if (response->body.reset) {
        ParserFactory::Reset();
        // 解析器Reset过程中需要把条件变量设置为true，所以结束后需要复位
        ParserStatusManager::Instance().ResetParse();
    }
    std::for_each(projectExplorerInfo.begin(), projectExplorerInfo.end(), [&request, &response](const auto &project) {
        auto projectEnum = static_cast<ProjectTypeEnum>(project.projectType);
        ParserType parserType = coverProjectTypeToParserType(projectEnum);
        auto parser = ParserFactory::GetProjectParser(parserType);
        parser->Parser({project}, request, *response);
    });
    ProjectParserBase::SendImportActionRes(std::move(response));
    ParserStatusManager::Instance().NotifyStartParse();
    return true;
}

bool ImportActionHandler::ImportFile(ImportActionRequest &request, std::string &warnMsg) {
    // 如果入参的文件内容不为空，则通过文件判断文件类型获取工厂
    std::string importPath = request.params.path[0];
    auto parserList = GetParserTypeList(importPath);
    // 联合导入，将不同类型的数据视为多个工程，降低改动成本
    auto response = std::make_unique<ImportActionResponse>();
    response->body.reset = IsNeedReset(request);
    if (response->body.reset) {
        ParserFactory::Reset();
    }
    auto invalid = std::all_of(parserList.begin(), parserList.end(), [&request, &warnMsg, &response](ParserType type) {
        auto project = BuildProjectInfo(type, request, warnMsg);
        if (!project) {
            return false;
        }
        auto parser = ParserFactory::GetProjectParser(type);
        if (parser) {
            parser->Parser({project.value()}, request, *response);
        }
        return true;
    });
    if (!invalid) {
        ServerLog::Warn("There is error occur when import");
        return false;
    }
    ProjectParserBase::SendImportActionRes(std::move(response));
    ParserStatusManager::Instance().NotifyStartParse();
    return true;
}

std::vector<ParserType> ImportActionHandler::GetParserTypeList(const std::string &importPath) {
    std::vector<ParserType> result;
    std::unique_ptr<ParserIE> ie = std::make_unique<Dic::Module::ParserIE>();
    bool existIE = ie->ExistIEFile(importPath);
    if (existIE) {
        result.emplace_back(ParserType::IE);
    }
    std::pair<std::string, ParserType> parserType = ParserFactory::GetImportType(importPath);
    // 只有IE文件时
    if (parserType.second == ParserType::OTHER && existIE) {
        return result;
    }
    result.emplace_back(parserType.second);
    return result;
}

std::optional<ProjectExplorerInfo> ImportActionHandler::BuildProjectInfo(
    ParserType allocType, ImportActionRequest &request, std::string &warnMsg) {
    std::string importPath = request.params.path[0];
    std::shared_ptr<ProjectParserBase> projectParser = ParserFactory::GetProjectParser(allocType);
    // 路径列表不为空，需要进行文件目录的新增、覆盖
    ProjectTypeEnum projectType = projectParser->GetProjectType(importPath);
    // 获取文件列表
    std::vector<std::string> tempFiles = projectParser->GetParseFileByImportFile(importPath, warnMsg);
    std::vector<std::string> parseFileList = tempFiles;
    bool isNotCluster = parseFileList.size() == 1 && !ClusterFileParser::CheckIsCluster(parseFileList[0]);
    // 如果未发生异常（warnMsg为空）或集群数据（容忍异常），进行错误重置，否则返回前端
    if (warnMsg.empty() || !isNotCluster) {
        // 重置（清空）
        SetTimelineError(ErrorCode::RESET_ERROR);
    }
    ProjectExplorerInfo project;
    project.fileName = importPath;
    project.projectName = request.params.projectName;
    project.projectType = static_cast<int64_t>(projectType);
    project.importType = "import";
    project.accessTime = TimeUtil::Instance().NowStr();
    ProjectAnalyze::Instance().ProjectExportInfoBuild(allocType, parseFileList, project);
    if (!Global::ProjectExplorerManager::Instance().SaveProjectExplorer({project}, request.params.isConflict)) {
        return std::nullopt;
    }
    LogIfFileNotExist(project);
    return project;
}
bool ImportActionHandler::IsNeedReset(const Protocol::ImportActionRequest &request) {
    // 如果是切换项目，则必须重置
    if (request.params.projectAction == ProjectActionEnum::TRANSFER_PROJECT) {
        return true;
    }
    // 新增文件时，以下情况需要对当前导入内容进行重置：1.导入数据和原来数据有冲突；2.无冲突，但是当前选中项目与目标项目不一致；
    std::string curProjectName = request.projectName;
    if (request.params.isConflict || (!curProjectName.empty() && curProjectName != request.params.projectName)) {
        return true;
    }
    return false;
}
