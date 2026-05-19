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

#include "pch.h"
#include "DataBaseManager.h"
#include "EventParser.h"
#include "ParserStatusManager.h"
#include "TraceTime.h"
#include "ClusterParseThreadPoolExecutor.h"
#include "EventNotifyThreadPoolExecutor.h"
#include "CacheManager.h"
#include "TrackInfoManager.h"
#include "BaselineManager.h"
#include "TraceFileParser.h"
#include <algorithm>

#include "ParseUnitManager.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool TraceFileParser::Parse(const std::vector<std::string> &filePathArr,
                            const std::string &rankId,
                            const std::string &selectedFolder,
                            const std::string &fileId)
{
    ServerLog::Info("start parse. file id:", fileId);
    ParserStatusManager::Instance().SetParserStatus(rankId, ParserStatus::INIT);
    auto preTask = [this, filePathArr, rankId = std::string(rankId), fileId = std::string(fileId)]()
    {
        PreParseTask(filePathArr, rankId, fileId);
    };
    threadPool_->AddTask(preTask, TraceIdManager::GetTraceId());
    return true;
}

void TraceFileParser::PreParseTask(const std::vector<std::string> &filePathArr,
                                   const std::string &rankId,
                                   const std::string &fileId)
{
    ParserStatusManager::Instance().WaitStartParse();
    if (!InitParser(filePathArr, rankId, fileId)) {
        auto msg = "Failed to open db. Please delete dbFile and try again or see logs in " +
                   ServerLog::GetCurrentLogPath();
#if defined(__linux__) || defined(__APPLE__)
        msg += FILE_DESCRIPTOR_RUN_OUT_MESSAGE;
#endif
        ParseEndCallBack(rankId, "", false, msg);
    }
}

bool TraceFileParser::CheckInitParser(const std::string &fileId)
{
    if (!ParserStatusManager::Instance().SetRunningStatus(fileId)) {
        ServerLog::Info("Pre task skip this file.");
        return false;
    }
    return true;
}

bool TraceFileParser::InitParser(const std::vector<std::string> &filePathArr,
                                 const std::string &rankId,
                                 const std::string &fileId)
{
    if (!CheckInitParser(rankId)) {
        return false;
    }
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(fileId);
    if (db == nullptr) {
        ServerLog::Error("Failed to get connection.");
        return false;
    }
    auto database = std::dynamic_pointer_cast<TextTraceDatabase, VirtualTraceDatabase>(db);
    if (database == nullptr) {
        ServerLog::Error("Failed to open trace database. rankId:", rankId);
        return false;
    }
    std::string statusInfo = ComputeStatusInfoFromPathArr(filePathArr);
    if ((database->HasFinishedParseLastTime(statusInfo) &&
        !Global::BaselineManager::Instance().IsBaselineRankId(rankId)) ||
        StringUtil::EndWith(filePathArr[0], "profiler.db") ||
        StringUtil::EndWith(filePathArr[0], "ms_service_parsed.db") ||
        StringUtil::EndWith(filePathArr[0], "ftrace_mindstudio_insight_data.db")) {
        uint64_t min = UINT64_MAX;
        uint64_t max = 0;
        if (!database->QueryExtremumTimestamp(min, max)) {
            return false;
        }
        database->CreateIndex();
        auto threadMap = database->QueryAllThreadMap();
        database->ExecSql("ALTER TABLE process ADD COLUMN parentPid TEXT DEFAULT '0';");
        TrackInfoManager::Instance().UpdateTrackIdMap(rankId, threadMap);
        Timeline::TraceTime::Instance().UpdateTime(min, 0);
        Timeline::TraceTime::Instance().UpdateCardTimeDuration(rankId, min, max);
        ParseEndCallBack(rankId, fileId, true, "");
        ParserStatusManager::Instance().SetFinishStatus(rankId);
        std::vector<std::string> taskStatusList = {CONNECTION_UNIT, WAIT_TIME_UNIT, OVERLAP_ANALYSIS_UNIT};
        for (const auto &item: taskStatusList) {
            database->UpdateValueIntoStatusInfoTable(item, FINISH_STATUS);
            ProjectParserBase::SendUnitFinishNotify(fileId, true, item);
        }
        ParseUnitManager::Instance().ExecuteUnitList({rankId}, FTRACE_STATUS_LIST);
        // FIX: 修复单 JSON 单 DeviceId 文件导入时，无法从文件路径知道 rankId deviceId 映射的问题，再次更新 rankIdToDeviceIdMap
        if (DataBaseManager::Instance().GetDeviceIdFromRankId(rankId).empty()) {
            UpdateRankIdDeviceIdMapByProcessData(database, rankId);
        }
        return true;
    }
    if (!database->DropTable() || !database->CreateTable() || !database->UpdateParseStatus(NOT_FINISH_STATUS)) {
        ServerLog::Error("Failed to init trace database. rankId:", rankId);
        return false;
    }
    InitFileProcess(filePathArr, rankId);
    return true;
}

std::string TraceFileParser::ComputeStatusInfoFromPathArr(const std::vector<std::string> &filePathArr)
{
    std::vector<std::string> tempPathArr(filePathArr);
    std::sort(tempPathArr.begin(), tempPathArr.end());
    std::string statusInfo = StringUtil::join(tempPathArr, ",");
    return statusInfo;
}

void TraceFileParser::InitFileProcess(const std::vector<std::string> &filePathArr, const std::string &rankId)
{
    auto start = std::chrono::high_resolution_clock::now();
    std::shared_ptr<std::vector<std::future<void>>> futures = std::make_shared<std::vector<std::future<void>>>();
    for (const auto &filePath : filePathArr) {
        ServerLog::Info("Start parse. rank id:", rankId, ". path:", filePath);
        auto splitFile = SplitFile(filePath);
        fileProgressMap[rankId] = std::make_unique<FileProgress>(0, FileUtil::GetFileSize(filePath.c_str()));
        if (splitFile.empty()) {
            ServerLog::Error("Failed to split file. filePath: %", filePath);
            ParseEndCallBack(rankId, "", false, "Failed to split file: " + filePath);
            continue;
        }

        for (const auto &pos : splitFile) {
            auto task = [this, filePath = std::string(filePath), rankId = std::string(rankId), pos]()
            {
                ParseTask(filePath, rankId, pos); // 安全调用成员函数
            };
            auto future = threadPool_->AddTask(std::move(task), TraceIdManager::GetTraceId());
            futures->emplace_back(std::move(future));
        }
    }
    auto endTask = [this, rankId = std::string(rankId), filePathArr/* 按值捕获 */, futures, start]()
    {
        EndParseTask(rankId, filePathArr, futures, start); // 安全调用成员函数
    };
    threadPool_->AddTask(endTask, TraceIdManager::GetTraceId());
}

void TraceFileParser::ParseTask(const std::string &filePath, const std::string &rankId, std::pair<int64_t, int64_t> pos)
{
    if (ParserStatusManager::Instance().GetParserStatus(rankId) != ParserStatus::RUNNING) {
        ServerLog::Info("Parse task skip this file. ID:", rankId);
        return;
    }
    auto db = DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId);
    if (db == nullptr) {
        ServerLog::Warn("Failed to get connection when parse timeline json,ID: ", rankId);
        return;
    }
    std::shared_ptr<TextTraceDatabase> databasePtr =
            std::dynamic_pointer_cast<TextTraceDatabase, VirtualTraceDatabase>(db);
    if (databasePtr == nullptr) {
        ServerLog::Warn("Failed to get text connection when parse timeline json,ID: ", rankId);
        return;
    }
    EventParser eventParser(filePath, rankId, databasePtr);
    if (!eventParser.Parse(pos.first, pos.second)) {
        if (ParserStatusManager::Instance().SetTerminateStatus(rankId) == ParserStatus::RUNNING) {
            // 只发送一次解析失败事件
            ParseEndCallBack(rankId, "", false, eventParser.GetError());
        }
    }
    // 发送单卡解析进度事件
    std::unique_ptr<FileProgress> &curFileProgress = fileProgressMap[rankId];
    curFileProgress->AddToParsedSize(pos.second - pos.first);
    parseProgressCallback(rankId, curFileProgress->GetParsedSize(), curFileProgress->GetTotalSize(),
                                   curFileProgress->GetProgressPercentage());
}

void TraceFileParser::EndParseTask(const std::string &rankId, const std::vector<std::string> &filePathArr,
                                   std::shared_ptr<std::vector<std::future<void>>> futures,
                                   std::chrono::time_point<std::chrono::high_resolution_clock> start)
{
    if (ParserStatusManager::Instance().GetParserStatus(rankId) != ParserStatus::RUNNING) {
        ParserStatusManager::Instance().SetFinishStatus(rankId);
        ServerLog::Info("End parse task skip this file. ID:", rankId);
        return;
    }
    ServerLog::Info("Wait parse completed. ID:", rankId);
    for (const auto &future : *futures) {
        future.wait();
    }
    auto end = std::chrono::high_resolution_clock::now();
    ServerLog::Info("Parse completed. ID:", rankId,
                    " Cost time(ms): ", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    auto db = DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId);
    if (db == nullptr) {
        ServerLog::Error("Failed to get connection. fileId:", rankId);
        ParserStatusManager::Instance().SetFinishStatus(rankId);
        return;
    }
    auto database = std::dynamic_pointer_cast<TextTraceDatabase, VirtualTraceDatabase>(db);
    if (database == nullptr) {
        ServerLog::Error("Failed to convert virtual trace database to json trace database in end parse task.");
        return;
    }
    database->CreateIndex();
    database->DeleteEmptyThread();
    database->DeleteEmptyFlow();
    std::string statusInfo = ComputeStatusInfoFromPathArr(filePathArr);
    database->UpdateParseStatus(statusInfo);
    std::vector<std::string> taskStatusList = {CONNECTION_UNIT, WAIT_TIME_UNIT, OVERLAP_ANALYSIS_UNIT};
    std::string fileId = DataBaseManager::Instance().GetFileIdByRankId(rankId);
    for (const auto &item: taskStatusList) {
        database->UpdateValueIntoStatusInfoTable(item, FINISH_STATUS);
        ProjectParserBase::SendUnitFinishNotify(fileId, true, item);
    }
    ServerLog::Info("Update depth completed. ID:", rankId);
    ParseUnitManager::Instance().ExecuteUnitList({rankId}, FTRACE_STATUS_LIST);
    if (PostParse(database, rankId)) {
        ParserStatusManager::Instance().SetFinishStatus(rankId);
    } else {
        ParserStatusManager::Instance().SetTerminateStatus(rankId);
    }
    ParseEndCallBack(rankId, database->GetDbPath(), true, "");
}

void TraceFileParser::UpdateRankIdDeviceIdMapByProcessData(std::shared_ptr<TextTraceDatabase> db,
                                                           const std::string &rankId)
{
    // FIX: 修复单 JSON 文件导入 deviceId 使用的 rankId(即文件名)，在查询时转成 int 由于非数字所以默认为 0 的问题
    // 有的单 JSON 文件 process 上 label 写的是 `NPU {number}`。这个情况下 deviceId 应该是 number。
    // 因此需要更新 DataBaseManager 中的 rankIdToDeviceIdMap [key: fileId + rankId , value: deviceId] 映射表
    // 1. 获取 process 列表
    const std::vector<Process> processes = db->QueryAllProcess();
    if (processes.empty()) {
        Server::ServerLog::Warn("No found processes");
        return;
    }
    // 2. 获取 label 中的 number 的集合
    std::unordered_set<std::string> uniqueNumbers;
    for (const auto& process : processes) {
        const std::string& label = process.label;
        if (StringUtil::StartWith(label, "NPU ")) {
            if (std::all_of(label.begin() + 4, label.end(), ::isdigit)) {
                // 提取数字部分并转换为整数
                std::string number = label.substr(4);
                uniqueNumbers.insert(number);
            }
            continue;
        }
        if (label == "NPU" || label == "") { // 对于 "NPU" label或者缺少label字段的数据，默认设置为 0
            uniqueNumbers.insert("0");
        }
    }
    // 3. 只处理 number 集合为 1 的情况：更新 DataBaseManager 中的 rankIdToDeviceIdMap
    if (uniqueNumbers.size() == 1) {
        const std::string deviceId = *uniqueNumbers.begin();
        const std::string fileId = DataBaseManager::Instance().GetFileIdByRankId(rankId);
        DataBaseManager::Instance().UpdateRankIdToDeviceId(fileId, rankId, deviceId);
    }
}

bool TraceFileParser::PostParse(std::shared_ptr<TextTraceDatabase> db, const std::string &rankId)
{
    // FIX: 修复单 JSON 单 DeviceId 文件导入时，无法从文件路径知道 rankId deviceId 映射的问题，再次更新 rankIdToDeviceIdMap
    if (DataBaseManager::Instance().GetDeviceIdFromRankId(rankId).empty()) {
        UpdateRankIdDeviceIdMapByProcessData(db, rankId);
    }
    return true; // do nothing
}

void TraceFileParser::ParseEndCallBack(const std::string &rankId,
                                       const std::string &fileId,
                                       bool result,
                                       const std::string &message)
{
    auto oldStatus = ParserStatusManager::Instance().GetParserStatus(rankId);
    if (parseEndCallback != nullptr && oldStatus != ParserStatus::TERMINATE) {
        parseEndCallback(rankId, fileId, result, message);
    }
}

int64_t TraceFileParser::GetTrackId(const std::string &fileId, const std::string &pid, const std::string &tid)
{
    std::unique_lock<std::mutex> lock(trackMutex);
    auto item = std::make_pair(pid, tid);
    if (trackIdMap[fileId].count(item) > 0) {
        return trackIdMap[fileId].at(item);
    }
    if (trackId == UINT64_MAX) {
        trackId = 0;
    }
    trackIdMap[fileId].emplace(item, ++trackId);
    return trackId;
}

void TraceFileParser::Reset()
{
    ServerLog::Info("Reset. wait task completed.");
    ParserStatusManager::Instance().SetAllTerminateStatus();
    threadPool_->Reset();
    ClusterParseThreadPoolExecutor::Instance().GetThreadPool()->Reset();
    EventNotifyThreadPoolExecutor::Instance().GetThreadPool()->Reset();
    ServerLog::Info("Task completed.");
    auto connList = DataBaseManager::Instance().GetAllTraceDatabase();
    for (auto &conn : connList) {
        conn->Stop();
    }
    DataBaseManager::Instance().ClearClusterDb();
    trackIdMap.clear();
    TrackInfoManager::Instance().Reset();
    trackId = 0;
    DataBaseManager::Instance().Clear();
    TraceTime::Instance().Reset();
    FileParser::Reset();
    ParserStatusManager::Instance().ClearAllParserStatus();
    CacheManager::Instance().ClearAll();
    ServerLog::Info("End Reset trace Parser");
}

void TraceFileParser::DeleteParseFiles(const std::vector<std::string> &rankIds)
{
    for (const auto &rankId : rankIds) {
        auto status = ParserStatusManager::Instance().SetTerminateStatus(rankId);
        auto kernelStatus = ParserStatusManager::Instance().SetTerminateStatus(KERNEL_PREFIX + rankId);
        auto memoryStatus = ParserStatusManager::Instance().SetTerminateStatus(MEMORY_PREFIX + rankId);
        ServerLog::Info("Before delete file. id:", rankId, ", status:", static_cast<int>(status),
            ", kernelStatus:", static_cast<int>(kernelStatus), ", memoryStatus:", static_cast<int>(memoryStatus));
    }
    ParserStatusManager::Instance().WaitAllFinished(rankIds);
    for (const auto &rankId : rankIds) {
        auto oldStatus = ParserStatusManager::Instance().GetParserStatus(rankId);
        ServerLog::Info("Clear cache. id:", rankId, ", status:", static_cast<int>(oldStatus));
        CacheManager::Instance().ClearCacheByRankId(rankId);
    }
}


} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
