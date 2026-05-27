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
#include <ProjectAnalyze.h>
#include "pch.h"
#include "CommonDefs.h"
#include "MemSnapshotParser.h"
#include "ProjectParserMemSnapshot.h"

namespace Dic::Module {
void ProjectParserMemSnapshot::Parser(const std::vector<ProjectExplorerInfo> &projectInfos,
    ImportActionRequest &request, ImportActionResponse &response) {
    ModuleRequestHandler::SetBaseResponse(request, response);
    if (std::empty(projectInfos)) {
        SendParseFailEvent("", "", "Project explorer info is not existed.");
        // 这里需要返回一个true应答,否则前端会陷入不停loading中
        return;
    }

    response.command = Protocol::REQ_RES_IMPORT_ACTION;
    response.moduleName = MODULE_TIMELINE; // 开始解析的响应中模块暂时必须设置为timeline
    response.body.reset = true;
    response.body.subParseFileInfo = projectInfos[0].subParseFileInfo;
    response.body.isLeaks = true;
    if (!MemSnapshotParser::Instance().GetParseContext().IsReadyToParse()) {
        response.result = false;
        return;
    }
    // 导入Snapshot单文件时，只会有二层文件树，且二级目录数量为1，因此这里直接对rankId进行赋值
    for (auto &item : projectInfos[0].projectFileTree) {
        for (auto &subItem : item->subParseFile) {
            subItem->rankId = subItem->parseFilePath;
        }
    }
    ModuleRequestHandler::SetResponseResult(response, true);
    if (!Global::ProjectExplorerManager::Instance().UpdateParseFileInfo(
            projectInfos[0].projectName, projectInfos[0].subParseFileInfo)) {
        Server::ServerLog::Error("Failed to update project in parsing");
        response.result = false;
        return;
    }
    response.body.projectFileTree = projectInfos[0].projectFileTree;
    SetBaseActionOfResponse(response, projectInfos[0].fileName, projectInfos[0].fileName, projectInfos[0].fileName,
        {projectInfos[0].fileName}, static_cast<int64_t>(ProjectTypeEnum::DB_MEMSCOPE));
    MemSnapshotParser::Instance().AsyncParseMemSnapshotPickle(projectInfos[0].fileName);
    response.result = true;
}

ProjectTypeEnum ProjectParserMemSnapshot::GetProjectType(const std::string &dataPath) {
    return ProjectTypeEnum::PKL_MEM_SNAPSHOT;
}

std::vector<std::string> ProjectParserMemSnapshot::GetParseFileByImportFile(
    const std::string &importFile, std::string &error) {
    // 注意importfile为完整路径
    if (FileUtil::IsFolder(importFile) || !FileUtil::CheckPathSecurity(importFile)) {
        error = "Supports import only from a single-file snapshot pickle.";
        return {};
    }
    return {importFile};
}

void ProjectParserMemSnapshot::BuildProjectExploreInfo(
    ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles) {
    ProjectParserBase::BuildProjectExploreInfo(projectInfo, parsedFiles);
    for (const auto &parsedFile : parsedFiles) {
        // snapshot类型没有上层结构
        auto parseFileInfo = std::make_shared<ParseFileInfo>();
        parseFileInfo->parseFilePath = parsedFile;
        parseFileInfo->type = ParseFileType::RANK;
        parseFileInfo->curDirName = FileUtil::GetFileName(parsedFile);
        parseFileInfo->subId = parsedFile;
        // fileId为原db
        parseFileInfo->fileId = parsedFile;
        parseFileInfo->rankId = parsedFile;
        projectInfo.AddSubParseFileInfo(parseFileInfo);
    }
}

bool ProjectParserMemSnapshot::IsSnapshotPickleFile(const std::string &filename) {
    const std::string lowerFileName = StringUtil::ToLower(filename);
    return StringUtil::EndWith(lowerFileName, pickleSuffix) ||
        StringUtil::EndWith(lowerFileName, pickleAbbreviationSuffix);
}

static ProjectAnalyzeRegister<ProjectParserMemSnapshot> pRegMemSnapshot(ParserType::PKL_MEM_SNAPSHOT);
}
