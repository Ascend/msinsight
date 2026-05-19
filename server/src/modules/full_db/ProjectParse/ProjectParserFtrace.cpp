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

#include "ProjectParserFtrace.h"
#include "ModuleRequestHandler.h"
#include "DataBaseManager.h"
#include "ParserStatusManager.h"
#include "EventNotifyThreadPoolExecutor.h"
#include "ProjectAnalyze.h"
#include "TrackInfoManager.h"
#include "CommonDefs.h"
#include "Database.h"
#include "JsonFileParserManager.h"

namespace Dic::Module {
using namespace Timeline;
using namespace Dic::Server;
using namespace Dic::Module::Global;

void ProjectParserFtrace::Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos,
                                  ImportActionRequest &request,
                                  ImportActionResponse &response)
{
    Timeline::DataBaseManager::Instance().SetDataType(Timeline::DataType::TEXT, request.params.path[0]);
    FillBaseResponseInfo(request, response, projectInfos);
    
    std::unordered_map<std::string, std::string> rankListMap;
    for (const auto &project : projectInfos) {
        for (const auto &parseFileInfo : project.subParseFileInfo) {
            std::string file = parseFileInfo->parseFilePath;
            std::string filename = FileUtil::GetFileName(file);
            if (!IsFtraceDbFile(filename)) {
                continue;
            }
            
            std::string rankId = file;
            std::string fileId = file;
            
            std::recursive_mutex sqlMutex;
            std::unique_ptr<Database> tempDataBase = std::make_unique<Database>(sqlMutex);
            tempDataBase->OpenDb(file, false);
            tempDataBase->SetDataBaseVersion();
            
            parseFileInfo->rankId = rankId;
            parseFileInfo->fileId = fileId;
            
            RankInfo rankInfo;
            rankInfo.rankId = rankId;
            rankInfo.rankName = rankId;
            TrackInfoManager::Instance().SetRankListByFileId(fileId, rankInfo);
            
            if (!DataBaseManager::Instance().CreateTraceConnectionPool(rankId, fileId)) {
                ServerLog::Error("Failed to create connection pool for ftrace db. file:", file);
                continue;
            }
            
            auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId);
            if (database == nullptr) {
                ServerLog::Error("Failed to get trace database for ftrace db. rankId:", rankId);
                continue;
            }
            
            rankListMap[rankId] = file;
            SetBaseActionOfResponse(response, rankId, fileId, fileId, {fileId},
                                    static_cast<int>(ProjectTypeEnum::DB_FTRACE));
        }
    }

    SetParseCallBack();
    if (rankListMap.size() >= PENDIND_CRITICAL_VALUE) {
        response.body.isPending = true;
    }
    response.body.isFtrace = true;
    ModuleRequestHandler::SetResponseResult(response, true);
    
    ThreadPool::Instance().AddTask(ProjectParserFtrace::ParserFtraceData, TraceIdManager::GetTraceId(), rankListMap);
}

void ProjectParserFtrace::FillBaseResponseInfo(const ImportActionRequest &request, ImportActionResponse &response,
                                                const std::vector<ProjectExplorerInfo> &projectInfos)
{
    ModuleRequestHandler::SetBaseResponse(request, response);
    response.body.subParseFileInfo.insert(response.body.subParseFileInfo.end(),
                                          projectInfos[0].subParseFileInfo.begin(),
                                          projectInfos[0].subParseFileInfo.end());
    MergeFileTree(response.body.projectFileTree, projectInfos[0].projectFileTree);
    response.command = Protocol::REQ_RES_IMPORT_ACTION;
    response.moduleName = MODULE_TIMELINE;
}

void ProjectParserFtrace::ParserFtraceData(const std::unordered_map<std::string, std::string> &rankListMap)
{
    ParserStatusManager::Instance().WaitStartParse();
    bool isParseTraceJson = rankListMap.size() < PENDIND_CRITICAL_VALUE;
    for (const auto &rankEntry : rankListMap) {
        if (!isParseTraceJson) {
            ParserStatusManager::Instance().SetPendingStatus(rankEntry.first,
                { ProjectTypeEnum::DB_FTRACE, { rankEntry.second } });
            continue;
        }
        Timeline::JsonFileParserManager::GetTraceFileParser().Parse({rankEntry.second},
                                                                    rankEntry.first,
                                                                    rankEntry.second,
                                                                    rankEntry.second);
    }
    Timeline::EventNotifyThreadPoolExecutor::Instance().GetThreadPool()->AddTask(
        SendAllParseSuccess, TraceIdManager::GetTraceId());
}

ProjectTypeEnum ProjectParserFtrace::GetProjectType(const std::string &dataPath)
{
    return ProjectTypeEnum::DB_FTRACE;
}

std::vector<std::string> ProjectParserFtrace::GetParseFileByImportFile(const std::string &importFile, std::string &error)
{
    if (!FileUtil::IsFolder(importFile)) {
        return {importFile};
    }

    auto traceFiles = FileUtil::FindFilesWithFilter(importFile, std::regex(ftraceDbReg));
    if (traceFiles.empty()) {
        error = "No ftrace db files found";
        ServerLog::Info(error);
        return {importFile};
    }
    
    return traceFiles;
}

bool ProjectParserFtrace::IsFtraceDbFile(const std::string &filename)
{
    return RegexUtil::RegexMatch(filename, ftraceDbReg).has_value();
}

void ProjectParserFtrace::BuildProjectExploreInfo(ProjectExplorerInfo &projectInfo,
                                                    const std::vector<std::string> &parsedFiles)
{
    ProjectParserBase::BuildProjectExploreInfo(projectInfo, parsedFiles);
    std::for_each(parsedFiles.begin(), parsedFiles.end(), [&projectInfo](const std::string &file) {
        auto parseFileInfoRank = std::make_shared<ParseFileInfo>();
        parseFileInfoRank->parseFilePath = file;
        parseFileInfoRank->type = ParseFileType::RANK;
        parseFileInfoRank->subId = FileUtil::GetFileName(file);
        parseFileInfoRank->curDirName = FileUtil::GetFileName(file);
        parseFileInfoRank->projectType = static_cast<int>(ProjectTypeEnum::DB_FTRACE);
        projectInfo.AddSubParseFileInfo(projectInfo.fileName, ParseFileType::PROJECT, parseFileInfoRank);
    });
}

void ProjectParserFtrace::SetParseCallBack()
{
    std::function<void(const std::string, const std::string, bool, const std::string)> func =
        std::bind(ParseEndCallBack, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3, std::placeholders::_4);
    Timeline::JsonFileParserManager::GetTraceFileParser().SetParseEndCallBack(func);

    std::function<void(const std::string, uint64_t parsedSize, uint64_t totalSize, int progress)> progressFunc =
        std::bind(ParseProgressCallBack, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                  std::placeholders::_4);
    Timeline::JsonFileParserManager::GetTraceFileParser().SetParseProgressCallBack(progressFunc);
}

ProjectAnalyzeRegister<ProjectParserFtrace> pRegFtrace(ParserType::DB_FTRACE);

} // Module
// Dic
