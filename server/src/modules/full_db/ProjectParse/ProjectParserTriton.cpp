// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
//  *
//  * MindStudio is licensed under Mulan PSL v2.
//  * You can use this software according to the terms and conditions of the Mulan PSL v2.
//  * You may obtain a copy of Mulan PSL v2 at:
//  *
//  *          http://license.coscl.org.cn/MulanPSL2
//  *
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//  * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//  * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//  * See the Mulan PSL v2 for more details.
//  * -------------------------------------------------------------------------
//  *

#include "ProjectParserTriton.h"
#include <string_view>
#include "ProjectAnalyze.h"
#include "ProjectExplorerManager.h"
#include "TritonParser.h"
#include "ThreadPool.h"

namespace Dic::Module {
void ProjectParserTriton::Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos,
    ImportActionRequest &request, ImportActionResponse &response) {
    ModuleRequestHandler::SetBaseResponse(request, response);
    if (projectInfos.empty()) {
        SendParseFailEvent("", "", "Project explorer info is not existed.");
        return;
    }

    response.command = Protocol::REQ_RES_IMPORT_ACTION;
    response.moduleName = MODULE_TIMELINE;
    response.body.reset = true;
    response.body.subParseFileInfo = projectInfos[0].subParseFileInfo;
    response.body.isTriton = true;
    ModuleRequestHandler::SetResponseResult(response, true);
    if (!Global::ProjectExplorerManager::Instance().UpdateParseFileInfo(
            projectInfos[0].projectName, projectInfos[0].subParseFileInfo)) {
        Server::ServerLog::Error("Failed to update project in parsing");
        response.result = false;
        SendParseFailEvent("", "", "Failed to update project in parsing");
        return;
    }
    response.body.projectFileTree = projectInfos[0].projectFileTree;
    SetBaseActionOfResponse(response, projectInfos[0].fileName, projectInfos[0].fileName, projectInfos[0].fileName,
        {projectInfos[0].fileName}, static_cast<int64_t>(ProjectTypeEnum::TRITON));
    ThreadPool::Instance().AddTask(
        [](const std::string &parseFile) { Dic::Module::Triton::TritonParser::Instance().Parse(parseFile); },
        TraceIdManager::GetTraceId(), projectInfos[0].subParseFileInfo[0]->parseFilePath);
}
ProjectTypeEnum ProjectParserTriton::GetProjectType(const std::string &dataPath) { return ProjectTypeEnum::TRITON; }

std::vector<std::string> ProjectParserTriton::GetParseFileByImportFile(
    const std::string &importFile, std::string &error) {
    if (FileUtil::IsFolder(importFile)) {
        Server::ServerLog::Error("Import file is a folder, please select a file.");
        return {};
    }
    if (IsTritonMemoryFile(importFile)) {
        return {importFile};
    }
    return {};
}

void ProjectParserTriton::BuildProjectExploreInfo(
    ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles) {
    ProjectParserBase::BuildProjectExploreInfo(projectInfo, parsedFiles);
    for (const auto &parsedFile : parsedFiles) {
        auto parsefileInfo = std::make_shared<ParseFileInfo>();
        parsefileInfo->parseFilePath = parsedFile;
        parsefileInfo->type = ParseFileType::RANK;
        parsefileInfo->subId = parsedFile;
        parsefileInfo->curDirName = FileUtil::GetFileName(parsedFile);
        parsefileInfo->rankId = parsedFile;
        projectInfo.subParseFileInfo.emplace_back(parsefileInfo);
        projectInfo.AddSubParseFileInfo(parsefileInfo);
    }
}
bool ProjectParserTriton::IsTritonMemoryFile(const std::string &file) {
    const std::string filename{"memory_info.json"};
    return file.find(filename) != std::string::npos;
}
static ProjectAnalyzeRegister<ProjectParserTriton> pRegTriton(ParserType::TRITON_MEMORY);
// Module
} // Dic
