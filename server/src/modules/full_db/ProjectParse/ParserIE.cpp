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
#include "ParserIE.h"
#include "ModuleRequestHandler.h"
#include "DataBaseManager.h"
#include "ParserStatusManager.h"
#include "EventNotifyThreadPoolExecutor.h"
#include "ServitizationOpenApi.h"
#include "ProjectAnalyze.h"
#include "TrackInfoManager.h"
#include "JsonFileParserManager.h"

namespace Dic::Module {
using namespace Timeline;
using namespace Dic::Server;
ParserIE::~ParserIE() = default;

void ParserIE::Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos, ImportActionRequest &request,
    ImportActionResponse &response) {
    Timeline::DataBaseManager::Instance().SetDataType(Timeline::DataType::TEXT, request.params.path[0]);
    // 基础信息填充
    FillBaseResponseInfo(request, response, projectInfos);
    // 获取rankid及文件映射关系信息
    std::unordered_map<std::string, std::string> rankListMap = GetRankListMap(projectInfos);
    // 设置基础响应内容
    for (const auto &rankEntry : rankListMap) {
        auto folders = rankEntry.second;
        if (rankEntry.second.empty()) {
            continue;
        }
        std::string cardPath = FileUtil::GetRankIdFromPath(rankEntry.second);
        SetBaseActionOfResponse(response, rankEntry.first, rankEntry.second, cardPath,
            {FileUtil::GetParentPath(folders)}, static_cast<int>(ProjectTypeEnum::IE));
    }
    // 解析内容
    SetParseCallBack(Timeline::JsonFileParserManager::GetTraceFileParser());
    if (rankListMap.size() >= PENDIND_CRITICAL_VALUE) {
        response.body.isPending = true;
    }
    response.body.isIE = true;
    ModuleRequestHandler::SetResponseResult(response, true);

    ThreadPool::Instance().AddTask(ParserIE::ParserTraceData, TraceIdManager::GetTraceId(), rankListMap);
}

void ParserIE::FillBaseResponseInfo(const ImportActionRequest &request, ImportActionResponse &response,
    const std::vector<ProjectExplorerInfo> &projectInfos) {
    ModuleRequestHandler::SetBaseResponse(request, response);
    response.body.subParseFileInfo.insert(response.body.subParseFileInfo.end(),
        projectInfos[0].subParseFileInfo.begin(), projectInfos[0].subParseFileInfo.end());
    MergeFileTree(response.body.projectFileTree, projectInfos[0].projectFileTree);
    response.command = Protocol::REQ_RES_IMPORT_ACTION;
    response.moduleName = MODULE_TIMELINE;
}

std::unordered_map<std::string, std::string> ParserIE::GetRankListMap(
    const std::vector<Global::ProjectExplorerInfo> &projectInfos) {
    // 获取单卡文件，并根据单卡所在目录获取其单卡信息
    std::unordered_map<std::string, std::string> rankToTraceMap;
    for (const auto &project : projectInfos) {
        for (const auto &parseFileInfo : project.subParseFileInfo) {
            std::vector<IE::TaskInfo> tasks = servitizationOpenApi->ComputeTaskInfo(parseFileInfo->parseFilePath);
            if (tasks.empty()) {
                return rankToTraceMap;
            }
            std::string fileId = tasks[0].fileId;
            rankToTraceMap[fileId] = tasks[0].filePath;
            std::recursive_mutex sqlMutex;
            std::unique_ptr<Database> tempDataBase = std::make_unique<Database>(sqlMutex);
            tempDataBase->OpenDb(tasks[0].filePath, false);
            tempDataBase->SetDataBaseVersion();
            parseFileInfo->rankId = fileId;
            parseFileInfo->fileId = tasks[0].filePath;
            RankInfo rankInfo;
            rankInfo.rankId = fileId;
            rankInfo.rankName = fileId;
            TrackInfoManager::Instance().SetRankListByFileId(tasks[0].filePath, rankInfo);
            if (!DataBaseManager::Instance().CreateTraceConnectionPool(fileId, tasks[0].filePath)) {
                ServerLog::Error("Failed to create connection pool. fileId:", fileId);
                return rankToTraceMap;
            }
            auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(fileId);
            if (database == nullptr) {
                return rankToTraceMap;
            }
        }
    }
    return rankToTraceMap;
}

void ParserIE::ParserTraceData(const std::unordered_map<std::string, std::string> &rankListMap) {
    // 对metadata数据进行解析
    ParserStatusManager::Instance().WaitStartParse();
    auto serverApi = std::make_shared<IE::ServitizationOpenApi>();
    serverApi->Parse(rankListMap);
    bool isParseTraceJson = rankListMap.size() < PENDIND_CRITICAL_VALUE;
    for (const auto &rankEntry : rankListMap) {
        if (!isParseTraceJson) {
            ParserStatusManager::Instance().SetPendingStatus(
                rankEntry.first, {ProjectTypeEnum::IE, {rankEntry.second}});
            continue;
        }
        Timeline::JsonFileParserManager::GetTraceFileParser().Parse(
            {rankEntry.second}, rankEntry.first, rankEntry.second, rankEntry.second);
    }
    Timeline::EventNotifyThreadPoolExecutor::Instance().GetThreadPool()->AddTask(
        SendAllParseSuccess, TraceIdManager::GetTraceId());
}

void ParserIE::SetParseCallBack(FileParser &fileParser) {
    std::function<void(const std::string, const std::string, bool, const std::string)> func = std::bind(
        ParseEndCallBack, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    fileParser.SetParseEndCallBack(func);

    // 复用解析完成回调函数设置逻辑
    std::function<void(const std::string, uint64_t parsedSize, uint64_t totalSize, int progress)> progressFunc =
        std::bind(ParseProgressCallBack, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
            std::placeholders::_4);
    fileParser.SetParseProgressCallBack(progressFunc);
}

std::vector<std::string> ParserIE::FindIEFile(const std::string &path) {
    std::vector<IE::TaskInfo> tasks = servitizationOpenApi->ComputeTaskInfo(path);
    std::vector<std::string> res;
    for (const auto &item : tasks) {
        res.emplace_back(item.filePath);
    }
    return res;
}

ProjectTypeEnum ParserIE::GetProjectType(const std::string &dataPath) { return ProjectTypeEnum::IE; }

std::vector<std::string> ParserIE::GetParseFileByImportFile(const std::string &importFile, std::string &error) {
    // 如果是文件，直接返回
    if (!FileUtil::IsFolder(importFile)) {
        return {importFile};
    }
    auto traceFiles = FindIEFile(importFile);
    if (traceFiles.empty()) {
        ServerLog::Info(error);
        return {importFile};
    }
    // 将所有文件的父目录放到一个set集合中（利用set进行去重）
    std::set<std::string> resultSet;
    for (const auto &item : traceFiles) {
        resultSet.insert(FileUtil::GetParentPath(item));
    }
    if (resultSet.empty()) {
        return {importFile};
    }
    // 转换成vector返回
    std::vector<std::string> result(resultSet.begin(), resultSet.end());
    return result;
}

bool ParserIE::ExistIEFile(const std::string &file) {
    if (file.empty()) {
        return false;
    }
    std::vector<IE::TaskInfo> res = servitizationOpenApi->ComputeTaskInfo(file);
    return !res.empty();
}

void ParserIE::BuildProjectExploreInfo(ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles) {
    ProjectParserBase::BuildProjectExploreInfo(projectInfo, parsedFiles);
    std::for_each(parsedFiles.begin(), parsedFiles.end(), [&projectInfo](const std::string &file) {
        auto parseFileInfoRank = std::make_shared<ParseFileInfo>();
        parseFileInfoRank->parseFilePath = file;
        parseFileInfoRank->type = ParseFileType::RANK;
        parseFileInfoRank->subId = FileUtil::GetFileName(file);
        parseFileInfoRank->curDirName = FileUtil::GetFileName(file);
        parseFileInfoRank->projectType = static_cast<int>(ProjectTypeEnum::IE);
        projectInfo.AddSubParseFileInfo(projectInfo.fileName, ParseFileType::PROJECT, parseFileInfoRank);
    });
}

ProjectAnalyzeRegister<ParserIE> pRegIE(ParserType::IE);
} // Module
// Dic
