/*
* -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#include "ProjectParserMemScope.h"

#include <ProjectAnalyze.h>
#include "pch.h"
#include "CommonDefs.h"
#include "DataBaseManager.h"
#include "MemScopeParser.h"
#include "MemScopeService.h"

namespace Dic::Module {
using namespace Dic::Server;

void ProjectParserMemScope::Parser(const std::vector<ProjectExplorerInfo> &projectInfos, ImportActionRequest &request,
    ImportActionResponse &response) {
    ModuleRequestHandler::SetBaseResponse(request, response);
    if (std::empty(projectInfos)) {
        SendParseFailEvent("", "", "Project explorer info is not existed.");
        // 这里需要返回一个true应答,否则前端会陷入不停loading中
        return;
    }
    response.command = Protocol::REQ_RES_IMPORT_ACTION;
    response.moduleName = MODULE_TIMELINE; // 开始解析的响应中模块暂时必须设置为timeline
    response.body.reset = true;
    response.body.isLeaks = true;
    // 导入MemScope单文件时，只会有二层文件树，且二级目录数量为1，因此这里直接对rankId进行赋值
    for (auto &item : projectInfos[0].projectFileTree) {
        for (auto &subItem : item->subParseFile) {
            subItem->rankId = subItem->parseFilePath;
        }
    }
    ModuleRequestHandler::SetResponseResult(response, true);
    if (!Global::ProjectExplorerManager::Instance().UpdateParseFileInfo(
            projectInfos[0].projectName, projectInfos[0].subParseFileInfo)) {
        ServerLog::Error("Failed to update project in parsing");
        response.result = false;
        return;
    }
    response.body.projectFileTree = projectInfos[0].projectFileTree;
    MemScopeParser::Instance().AsyncParseMemScopeDbFile(projectInfos[0].fileName);
    SetBaseActionOfResponse(response, projectInfos[0].fileName, projectInfos[0].fileName, projectInfos[0].fileName,
        {projectInfos[0].fileName}, static_cast<int64_t>(ProjectTypeEnum::DB_MEMSCOPE));
}

ProjectTypeEnum ProjectParserMemScope::GetProjectType(const std::string &dataPath) {
    return ProjectTypeEnum::DB_MEMSCOPE;
}

// 1. projectParser识别导入目录下可解析的文件
std::vector<std::string> ProjectParserMemScope::GetParseFileByImportFile(
    const std::string &importFile, std::string &error) {
    // 注意importfile为完整路径
    if (FileUtil::IsFolder(importFile)) {
        error = "Supports import only from a single-file memscope database.";
        return {};
    }
    return {importFile};
}

// 2. 先于parser被调用，用于组织出一个完整的项目，该部分可能控制在前端目录树的展示
// parsedFiles = 待解析的所有文件或文件目录。
void ProjectParserMemScope::BuildProjectExploreInfo(
    ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles) {
    ProjectParserBase::BuildProjectExploreInfo(projectInfo, parsedFiles);
    for (const auto &parsedFile : parsedFiles) {
        // memscope类型没有上层结构
        auto parseFileInfo = std::make_shared<ParseFileInfo>();
        parseFileInfo->parseFilePath = parsedFile; // 原始导入数据目录
        parseFileInfo->type = ParseFileType::RANK;
        parseFileInfo->curDirName = FileUtil::GetFileName(parsedFile);
        parseFileInfo->subId = parsedFile; //
        // fileId为原db
        parseFileInfo->fileId = parsedFile; // 数据的key
        parseFileInfo->rankId = parsedFile;
        projectInfo.AddSubParseFileInfo(parseFileInfo);
    }
}

bool ProjectParserMemScope::IsMemScopeDbFile(const std::string &filename) {
    return std::regex_match(filename, std::regex(memScopeDbReg));
}

static ProjectAnalyzeRegister<ProjectParserMemScope> pRegMemScope(ParserType::DB_MEMSCOPE);
}
