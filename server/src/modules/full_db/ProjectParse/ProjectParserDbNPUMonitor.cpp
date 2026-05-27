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
#include "ProjectParserDbNPUMonitor.h"
#include "CommonDefs.h"
#include "ServerLog.h"
#include "DataBaseManager.h"
#include "ProjectAnalyze.h"
#include "FullDbParser.h"
#include "BaselineManager.h"

namespace Dic {
namespace Module {
using namespace Server;
using namespace FullDb;

ProjectTypeEnum ProjectParserDbNPUMonitor::GetProjectType(const std::string &dataPath) {
    return ProjectTypeEnum::DB_NPUMONITOR;
}

std::vector<std::string> ProjectParserDbNPUMonitor::GetParseFileByImportFile(
    const std::string &importFile, std::string &error) {
    std::vector<std::string> npuMonitorFiles = FileUtil::FindNPUMonitorFiles(importFile);
    if (npuMonitorFiles.empty()) {
        error = "No parsable db files found, Possible reasons:; 1.File not exist; "
                "2.The nesting depth of the imported sub-file exceeds 5; 3.The sub-file path length exceeds " +
            std::to_string(FileUtil::GetFilePathLengthLimit());
        ServerLog::Info(error);
        return {importFile};
    }
    // npumonitor的表结构和PTA的db格式相同
    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, importFile);
    return npuMonitorFiles;
}

void ProjectParserDbNPUMonitor::BuildProjectExploreInfo(
    Dic::Module::Global::ProjectExplorerInfo &info, const std::vector<std::string> &parsedFiles) {
    ProjectParserBase::BuildProjectExploreInfo(info, parsedFiles);
    for (const auto &parsedFile : parsedFiles) {
        auto parseFileInfoRank = std::make_shared<ParseFileInfo>();
        parseFileInfoRank->parseFilePath = parsedFile;
        parseFileInfoRank->type = ParseFileType::RANK;
        parseFileInfoRank->subId = parsedFile;
        parseFileInfoRank->curDirName = FileUtil::GetFileName(parsedFile);
        parseFileInfoRank->fileId = parsedFile;
        parseFileInfoRank->projectType = static_cast<int>(ProjectTypeEnum::DB_NPUMONITOR);
        info.AddSubParseFileInfo(info.fileName, ParseFileType::PROJECT, parseFileInfoRank);
    }
}

void ProjectParserDbNPUMonitor::ParserBaseline(
    const Global::ProjectExplorerInfo &projectInfo, Global::BaselineInfo &baselineInfo) {
    // 该函数主要复用ProjectParserDb::ParserBaseline()的逻辑，除去了找文件的步骤，因为npumonitor数据的parsedFilePath就是db文件的路径，不需要额外查找
    if (projectInfo.fileInfoMap.empty()) {
        return;
    }

    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, baselineInfo.parsedFilePath);
    Timeline::DataBaseManager::Instance().SetDataType(Timeline::DataType::DB, baselineInfo.parsedFilePath);
    bool isParsed = Timeline::DataBaseManager::Instance().IsContainDatabasePath(baselineInfo.parsedFilePath);
    auto hostInfoMap = GetReportFiles({projectInfo}, baselineInfo.parsedFilePath);
    if (std::empty(hostInfoMap)) {
        Global::BaselineManager::Instance().SetBaselineInfo(baselineInfo);
        baselineInfo.errorMessage = "Can't get host info, and failed to compare!";
        return;
    }
    FilterHostMap(hostInfoMap, baselineInfo.parsedFilePath);
    if (std::empty(hostInfoMap.begin()->second) || std::empty(hostInfoMap.begin()->second.begin()->second)) {
        Global::BaselineManager::Instance().SetBaselineInfo(baselineInfo);
        baselineInfo.errorMessage = "Can't get rank info, and failed to compare!";
        return;
    }
    std::string rankId = hostInfoMap.begin()->first + hostInfoMap.begin()->second.begin()->second[0];

    baselineInfo.rankId = rankId;
    baselineInfo.cardName = "Baseline_" + rankId;
    baselineInfo.host = hostInfoMap.begin()->first;
    baselineInfo.fileId = baselineInfo.parsedFilePath;
    Global::BaselineManager::Instance().SetBaselineInfo(baselineInfo);
    if (!Timeline::DataBaseManager::Instance().CreateTraceConnectionPool(
            baselineInfo.rankId, baselineInfo.parsedFilePath)) {
        ServerLog::Error("Failed to create baseline connection pool for NPU monitor. ");
    }
    if (isParsed) {
        ServerLog::Warn("Baseline has been parsed.");
        return;
    }
    FullDb::FullDbParser::Instance().Parse(std::vector<std::string>{baselineInfo.rankId}, baselineInfo.parsedFilePath);
}

ProjectAnalyzeRegister<ProjectParserDbNPUMonitor> pRegDBNPUMonitor(ParserType::DB_NPUMONITOR);
}
}
