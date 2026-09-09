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

#include "ProjectParserPytorchTrace.h"
#include "ProjectParserJson.h"
#include "JsonFileParserManager.h"
#include "ProjectAnalyze.h"

namespace Dic::Module {
namespace {
const std::regex PYTORCH_TRACE_REGEX(R"(^.+\.pt\.trace\.json$)");
const std::regex GENERIC_TRACE_REGEX(R"(^(trace_view|trace|msprof(_slice)?(_[0-9]{1,15}){1,4})\.json$)");

std::vector<std::string> FindPytorchTraceFiles(const std::string &path) {
    return FileUtil::FindAllFilesByRegex(path, PYTORCH_TRACE_REGEX);
}

std::vector<std::string> CollectPytorchTraceParseFiles(const std::string &importFile, std::string &error) {
    if (!FileUtil::IsFolder(importFile)) {
        if (ProjectParserPytorchTrace::IsPytorchTraceFile(importFile)) {
            return {importFile};
        }
        error = "The selected file is not a PyTorch trace JSON file";
        return {};
    }

    std::vector<std::string> traceFiles = FindPytorchTraceFiles(importFile);
    if (traceFiles.size() != 1) {
        error = traceFiles.empty() ? "No PyTorch trace JSON file found"
                                   : "Importing multiple PyTorch trace JSON files is not supported";
        return {};
    }
    if (!FileUtil::FindAllFilesByRegex(importFile, GENERIC_TRACE_REGEX).empty()) {
        error = "Importing PyTorch and regular trace JSON files together is not supported";
        return {};
    }
    return traceFiles;
}
} // namespace

ProjectParserPytorchTrace::ProjectParserPytorchTrace()
    : jsonParser(std::make_unique<ProjectParserJson>(Timeline::JsonFileParserManager::GetTraceFileParser())) {}

ProjectParserPytorchTrace::~ProjectParserPytorchTrace() = default;

void ProjectParserPytorchTrace::Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos,
    ImportActionRequest &request, ImportActionResponse &response) {
    jsonParser->Parser(projectInfos, request, response);
}

void ProjectParserPytorchTrace::ParserBaseline(
    const Global::ProjectExplorerInfo &projectInfo, Global::BaselineInfo &baselineInfo) {
    jsonParser->ParserBaseline(projectInfo, baselineInfo);
}

ProjectTypeEnum ProjectParserPytorchTrace::GetProjectType(const std::string &dataPath) {
    return ProjectTypeEnum::PYTORCH_TRACE;
}

std::vector<std::string> ProjectParserPytorchTrace::GetParseFileByImportFile(
    const std::string &importFile, std::string &error) {
    return CollectPytorchTraceParseFiles(importFile, error);
}

bool ProjectParserPytorchTrace::IsPytorchTraceFile(const std::string &path) {
    const std::string fileName = FileUtil::GetFileName(path);
    return fileName.size() > PT_TRACE_JSON_SUFFIX.size() && StringUtil::EndWith(fileName, PT_TRACE_JSON_SUFFIX);
}

std::string ProjectParserPytorchTrace::GetDirectFileDbPath(const std::string &filePath) {
    const std::string fileName = FileUtil::GetFileName(filePath);
    const std::string baseName = fileName.substr(0, fileName.size() - PT_TRACE_JSON_SUFFIX.size());
    return FileUtil::SplicePath(FileUtil::GetParentPath(filePath), baseName + "_mindstudio_insight_data.db");
}

bool ProjectParserPytorchTrace::HasPytorchTraceFile(const std::string &path) {
    std::string error;
    return !CollectPytorchTraceParseFiles(path, error).empty();
}

void ProjectParserPytorchTrace::BuildProjectExploreInfo(
    ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles) {
    ProjectParserBase::BuildProjectExploreInfo(projectInfo, parsedFiles);
    for (const auto &parsedFile : parsedFiles) {
        auto parseFileInfo = std::make_shared<ParseFileInfo>();
        parseFileInfo->parseFilePath = parsedFile;
        parseFileInfo->type = ParseFileType::RANK;
        parseFileInfo->subId = FileUtil::GetFileName(parsedFile);
        parseFileInfo->curDirName = FileUtil::GetFileName(parsedFile);
        parseFileInfo->projectType = static_cast<int64_t>(ProjectTypeEnum::PYTORCH_TRACE);
        parseFileInfo->fileId = FileUtil::IsFolder(projectInfo.fileName)
            ? FileUtil::SplicePath(FileUtil::GetParentPath(parsedFile), DATABASE_FILE_NAME)
            : ProjectParserPytorchTrace::GetDirectFileDbPath(parsedFile);
        projectInfo.AddSubParseFileInfo(projectInfo.fileName, ParseFileType::PROJECT, parseFileInfo);
    }
}

ProjectAnalyzeRegister<ProjectParserPytorchTrace> pRegPytorchTrace(ParserType::PYTORCH_TRACE_JSON);

} // namespace Dic::Module
