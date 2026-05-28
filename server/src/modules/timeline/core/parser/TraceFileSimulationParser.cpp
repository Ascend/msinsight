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
#include "CacheManager.h"
#include "ProjectParserFactory.h"
#include "TraceFileSimulationParser.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

TraceFileSimulationParser &TraceFileSimulationParser::Instance() {
    static TraceFileSimulationParser instance;
    return instance;
}

TraceFileSimulationParser::TraceFileSimulationParser() {
    threadPool = std::make_unique<ThreadPool>(TraceFileSimulationParser::maxThreadNum);
}

TraceFileSimulationParser::~TraceFileSimulationParser() { threadPool->ShutDown(); }

bool TraceFileSimulationParser::Parse(const std::vector<std::string> &filePathArr, const std::string &rankId,
    const std::string &selectedFolder, const std::string &fileId) {
    ServerLog::Info("start parse. file id:", rankId);
    ParserStatusManager::Instance().SetParserStatus(rankId, ParserStatus::INIT);
    threadPool->AddTask(PreParseTask, TraceIdManager::GetTraceId(), filePathArr, rankId, fileId);
    return true;
}

void TraceFileSimulationParser::PreParseTask(
    const std::vector<std::string> &filePathArr, const std::string &rankId, const std::string &fileId) {
    ParserStatusManager::Instance().WaitStartParse();
    if (!InitParser(filePathArr, rankId, fileId)) {
        auto msg = "Failed to open db. Please delete dbFile and try again or see logs.";
        ParseEndCallBack(rankId, fileId, false, msg);
    }
}

bool TraceFileSimulationParser::InitParser(
    const std::vector<std::string> &filePathArr, const std::string &rankId, const std::string &fileId) {
    if (!ParserStatusManager::Instance().SetRunningStatus(rankId)) {
        ServerLog::Info("Pre task skip this file.");
        return false;
    }
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(fileId);
    if (db == nullptr) {
        ServerLog::Error("Failed to get connection. fileId: ", fileId);
        return false;
    }
    std::shared_ptr<TextTraceDatabase> database =
        std::dynamic_pointer_cast<TextTraceDatabase, VirtualTraceDatabase>(db);
    if (database == nullptr) {
        ServerLog::Error("Failed to convert virtual trace database to json trace database in event parser.");
        return false;
    }
    if (!(database->DropTable() && database->CreateTable())) {
        ServerLog::Error("Failed to open trace database. rankId:", rankId);
        return false;
    }
    auto &instance = TraceFileSimulationParser::Instance();
    auto start = std::chrono::high_resolution_clock::now();
    std::shared_ptr<std::vector<std::future<void>>> futures = std::make_shared<std::vector<std::future<void>>>();
    for (const auto &filePath : filePathArr) {
        ServerLog::Info("Start parse. file id:", rankId, ". path:", filePath);
        auto splitFile = TraceFileSimulationParser::SplitFile(filePath);
        instance.fileProgressMap[rankId] = std::make_unique<FileProgress>(0, FileUtil::GetFileSize(filePath.c_str()));
        if (splitFile.empty()) {
            ServerLog::Error("Failed to split file. filePath: %", filePath);
            ParseEndCallBack(rankId, fileId, false, "Failed to split file: " + filePath);
            continue;
        }

        for (const auto &pos : splitFile) {
            auto future =
                instance.threadPool->AddTask(ParseTask, TraceIdManager::GetTraceId(), filePath, rankId, fileId, pos);
            futures->emplace_back(std::move(future));
        }
    }
    instance.threadPool->AddTask(
        EndParseTask, TraceIdManager::GetTraceId(), rankId, filePathArr, futures, start, fileId);
    return true;
}

void TraceFileSimulationParser::ParseTask(const std::string &filePath, const std::string &rankId,
    const std::string &fileId, std::pair<int64_t, int64_t> pos) {
    if (ParserStatusManager::Instance().GetParserStatus(rankId) != ParserStatus::RUNNING) {
        ServerLog::Info("Parse task skip this file. ID:", rankId);
        return;
    }
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(fileId);
    if (db == nullptr) {
        ServerLog::Warn("Failed to get connection when parse simulation json,ID: ", rankId);
        return;
    }
    std::shared_ptr<TextTraceDatabase> databasePtr =
        std::dynamic_pointer_cast<TextTraceDatabase, VirtualTraceDatabase>(db);
    if (databasePtr == nullptr) {
        ServerLog::Warn("Failed to get text connection when parse simulation json,ID: ", rankId);
        return;
    }
    EventParser eventParser(filePath, rankId, databasePtr);
    eventParser.SetSimulationStatus(true);
    if (!eventParser.Parse(pos.first, pos.second)) {
        if (ParserStatusManager::Instance().SetTerminateStatus(rankId) == ParserStatus::RUNNING) {
            // 只发送一次解析失败事件
            ParseEndCallBack(rankId, fileId, false, eventParser.GetError());
        }
    }
    // 发送单卡解析进度事件
    auto &instance = TraceFileSimulationParser::Instance();
    std::unique_ptr<FileProgress> &curFileProgress = instance.fileProgressMap[rankId];
    curFileProgress->AddToParsedSize(pos.second - pos.first);
    instance.parseProgressCallback(rankId, curFileProgress->GetParsedSize(), curFileProgress->GetTotalSize(),
        curFileProgress->GetProgressPercentage());
}

void TraceFileSimulationParser::EndParseTask(const std::string &rankId, const std::vector<std::string> &filePathArr,
    std::shared_ptr<std::vector<std::future<void>>> futures,
    std::chrono::time_point<std::chrono::high_resolution_clock> start, const std::string &fileId) {
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
    auto database = std::dynamic_pointer_cast<TextTraceDatabase, VirtualTraceDatabase>(
        DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId));
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection. fileId:", rankId);
        ParserStatusManager::Instance().SetFinishStatus(rankId);
        return;
    }
    database->CreateIndex();
    CacheManager::Instance().ClearCacheByRankId(rankId);
    ServerLog::Info("Update depth completed. ID:", rankId);
    ParseEndCallBack(rankId, fileId, true, "");
    // update flow status
    database->UpdateValueIntoStatusInfoTable(CONNECTION_UNIT, FINISH_STATUS);
    ProjectParserBase::SendUnitFinishNotify(fileId, true, CONNECTION_UNIT);
}

void TraceFileSimulationParser::ParseEndCallBack(
    const std::string &rankId, const std::string &fileId, bool result, const std::string &message) {
    auto oldStatus = ParserStatusManager::Instance().GetParserStatus(rankId);
    ParserStatusManager::Instance().SetFinishStatus(rankId);
    auto &instance = TraceFileSimulationParser::Instance();
    if (instance.parseEndCallback != nullptr && oldStatus != ParserStatus::TERMINATE) {
        ServerLog::Info("TraceFileSimulationParser send Message");
        instance.parseEndCallback(rankId, fileId, result, message);
    }
}

int64_t TraceFileSimulationParser::GetTrackId(
    const std::string &fileId, const std::string &pid, const std::string &tid) {
    std::unique_lock<std::mutex> lock(trackMutex);
    auto item = std::make_pair(pid, tid);
    if (trackIdMap[fileId].count(item) > 0) {
        return trackIdMap[fileId].at(item);
    }
    if (trackId == INT64_MAX) {
        trackId = 0;
    }
    trackIdMap[fileId].emplace(item, ++trackId);
    return trackId;
}

void TraceFileSimulationParser::Reset() {
    ServerLog::Info("Reset. wait task completed.");
    ParserStatusManager::Instance().SetAllTerminateStatus();
    threadPool->Reset();
    ServerLog::Info("Task completed.");
    auto connList = DataBaseManager::Instance().GetAllTraceDatabase();
    for (auto &conn : connList) {
        conn->Stop();
    }
    trackIdMap.clear();
    trackId = 0;
    DataBaseManager::Instance().Clear();
    TraceTime::Instance().Reset();
    FileParser::Reset();
    ParserStatusManager::Instance().ClearAllParserStatus();
    CacheManager::Instance().ClearAll();
    ServerLog::Info("End Reset trace Parser");
}

void TraceFileSimulationParser::DeleteParseFiles(const std::vector<std::string> &fileIds) {
    for (const auto &fileId : fileIds) {
        auto status = ParserStatusManager::Instance().SetTerminateStatus(fileId);
        ServerLog::Info("Before delete file. id:", fileId, ", status:", static_cast<int>(status));
    }
    ParserStatusManager::Instance().WaitAllFinished(fileIds);
    for (const auto &fileId : fileIds) {
        auto oldStatus = ParserStatusManager::Instance().GetParserStatus(fileId);
        ServerLog::Info("Clear Cache. id:", fileId, ", status:", static_cast<int>(oldStatus));
        CacheManager::Instance().ClearCacheByRankId(fileId);
    }
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
