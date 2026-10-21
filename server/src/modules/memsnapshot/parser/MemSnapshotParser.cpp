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

#include "FileUtil.h"
#include "PythonUtil.h"
#include "SafeFile.h"
#include "DataBaseManager.h"
#include "MemSnapshotParser.h"
#include "MemSnapshotDatabase.h"
#include "MemSnapshotSliceService.h"
#include "WsSender.h"

#include <chrono>
#include <cstdint>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#define SLEEP(ms) Sleep(ms)
#else
#define SLEEP(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#endif

namespace Dic::Module {
using namespace Dic::Module::Timeline;
using Dic::OpenReadFileSafely;
constexpr int MEM_SNAPSHOT_EVENTS_PER_SLICE = 500000;

void MemSnapshotParserContext::Reset(
    std::string nPicklePath, std::string nLogPath, std::string nOutputPath, std::string nFileHash) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    picklePath = std::move(nPicklePath);
    logPath = std::move(nLogPath);
    outputDbPath = std::move(nOutputPath);
    fileHash = std::move(nFileHash);
    state = ParserState::INIT;
    progress = 0;
    initialSuccessSent = false;
    initialSuccessSentWhileBuilding = false;
    workDir = FileUtil::GetCurrPath();
}

bool MemSnapshotParserContext::IsFinished() const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return state == ParserState::FINISH_FAILURE || state == ParserState::FINISH_SUCCESS ||
        state == ParserState::UP_TO_DATE;
}

bool MemSnapshotParserContext::IsReadyToParse() const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return state != ParserState::Loading && state != ParserState::Processing;
}

std::string MemSnapshotParserContext::GetPicklePath() const { return picklePath; }

std::string MemSnapshotParserContext::GetLogPath() const { return logPath; }

std::string MemSnapshotParserContext::GetOutputDbPath() const { return outputDbPath; }

std::string MemSnapshotParserContext::GetFileHash() const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return fileHash;
}

void MemSnapshotParserContext::SetFileHash(std::string hash) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    fileHash = std::move(hash);
}

bool MemSnapshotParserContext::IsInitialSuccessSent() const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return initialSuccessSent;
}

bool MemSnapshotParserContext::WasInitialSuccessSentWhileBuilding() const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return initialSuccessSentWhileBuilding;
}

void MemSnapshotParserContext::MarkInitialSuccessSent(bool parsingComplete) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    initialSuccessSent = true;
    initialSuccessSentWhileBuilding = !parsingComplete;
}

ParserState MemSnapshotParserContext::GetState() const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return state;
}

void MemSnapshotParserContext::SetState(const ParserState &newState) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    state = newState;
    Server::ServerLog::Info("Snapshot pickle file parse state changed: ", static_cast<int>(state));
}

uint8_t MemSnapshotParserContext::GetProgress() const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return progress;
}

void MemSnapshotParserContext::SetProgress(uint8_t newProgress) {
    uint8_t oldProgress = 0;
    bool emitEachPercent = false;
    {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        if (progress >= newProgress) {
            return;
        }
        oldProgress = progress;
        progress = newProgress;
        emitEachPercent = state == ParserState::Processing;
    }
    if (!emitEachPercent) {
        Server::ServerLog::Info("Snapshot pickle file parse progress changed: ", static_cast<int>(newProgress));
        SendParseProgressEvent(newProgress);
        return;
    }
    for (int current = static_cast<int>(oldProgress) + 1; current <= static_cast<int>(newProgress); ++current) {
        Server::ServerLog::Info("Snapshot pickle file parse progress changed: ", current);
        SendParseProgressEvent(current);
    }
}

void MemSnapshotParserContext::SendParseProgressEvent(int progress) {
    auto event = std::make_unique<MemSnapshotParseProgressEvent>();
    event->moduleName = Protocol::MODULE_MEM_SCOPE;
    event->result = true;
    event->body.fileId = MemSnapshotParser::Instance().GetParseContext().GetPicklePath();
    event->body.progress = progress;
    Dic::SendEvent(std::move(event));
}

std::string MemSnapshotParserContext::GetWorkDir() const { return workDir; }

MemSnapshotParser &MemSnapshotParser::Instance() {
    static MemSnapshotParser _instance;
    return _instance;
}

void MemSnapshotParser::Reset() {
    Server::ServerLog::Info("[Snapshot] Parser Reset.");
    _threadPool->Reset();
    parseContext.Reset();
    MemSnapshotDatabase::Reset();
}

void MemSnapshotParser::AsyncParseMemSnapshotPickle(const std::string &pickleFilePath) {
    const std::string realPicklePath = FileUtil::GetRealPath(pickleFilePath);
    const std::string &snapshotPath = realPicklePath.empty() ? pickleFilePath : realPicklePath;
    const std::string outputPath = MemSnapshotSliceService::GetArtifactDirectory(snapshotPath);
    const std::string logPath = MemSnapshotSliceService::GetLogPath(snapshotPath);
    parseContext.Reset(snapshotPath, logPath, outputPath);
    auto traceId = TraceIdManager::GenerateTraceId();
    Server::ServerLog::Info("[Snapshot] Parsing pickle file: %, log file: %, output db file: %.",
        parseContext.GetPicklePath(), parseContext.GetLogPath(), parseContext.GetOutputDbPath());
    _threadPool->AddTask(ParseMemSnapshotTask, traceId);
    _threadPool->AddTask(ParseDaemonTask, traceId);
}

MemSnapshotParserContext &MemSnapshotParser::GetParseContext() { return parseContext; }

MemSnapshotParser::MemSnapshotParser() {
    // MemSnapshot解析snapshot解析至少需要双线程
    _threadPool = std::make_unique<ThreadPool>(2);
}

MemSnapshotParser::~MemSnapshotParser() { _threadPool->ShutDown(); }

void MemSnapshotParser::ParseMemSnapshotTask() {
    Server::ServerLog::Info("[Snapshot] Parse snapshot thread started.");
    // 缓存校验、旧解析产物清理由脚本统一负责；这里只清理本次进度日志。
    MemSnapshotDatabase::Reset();
    const auto logPath = Instance().parseContext.GetLogPath();
    if (FileUtil::IsFilePathExist(logPath) && !FileUtil::RemoveFile(logPath)) {
        Server::ServerLog::Error("[Snapshot] Failed to remove stale parsing log: %.", logPath);
        Instance().parseContext.SetState(ParserState::FINISH_FAILURE);
        return;
    }
    const std::string memSnapDumpScriptsPath = FileUtil::SplicePath("mem_snap_dump", "tools", "dump2db.py");
    Server::ServerLog::Info("[Snapshot] Start parsing.");
    std::vector<std::string> arguments{Instance().parseContext.GetPicklePath(), "--dump_dir",
        Instance().parseContext.GetOutputDbPath(), "--log", Instance().parseContext.GetLogPath(), "--events_per_slice",
        std::to_string(MEM_SNAPSHOT_EVENTS_PER_SLICE)};
    Instance().parseContext.SetState(ParserState::Processing);
    try {
        Server::ServerLog::Info(
            "[Snapshot] Script: %, arguments: %", memSnapDumpScriptsPath, StringUtil::join(arguments, " "));
        const int result = PythonUtil::ExecuteScript(memSnapDumpScriptsPath, arguments);
        Server::ServerLog::Info("[Snapshot] Parsing finished.result = ", result);
        Instance().parseContext.SetState(result == 0 ? ParserState::FINISH_SUCCESS : ParserState::FINISH_FAILURE);
    } catch (...) {
        Server::ServerLog::Error("[Snapshot] Parsing finished.result = UNKNOWN_ERROR");
        Instance().parseContext.SetState(ParserState::FINISH_FAILURE);
    }
}

int ReadProgressInLogFile(std::ifstream &file, std::string &err) {
    // 读取最新进展
    const std::regex progressReg(R"((\d+(?:\.\d+)?)% of entries have been processed)");
    const std::string parseFailedKeyWord = "Failed to dump the snapshot to database.";
    const std::regex cacheReadyReg(R"(Snapshot cache (?:validated|rebuild): ([0-9a-f]{64}))");
    std::string line;
    std::string lastProgressLine;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        if (StringUtil::Contains(line, parseFailedKeyWord)) {
            err = line;
            return -1;
        }
        std::smatch cacheMatch;
        if (std::regex_search(line, cacheMatch, cacheReadyReg)) {
            MemSnapshotParser::Instance().GetParseContext().SetFileHash(cacheMatch[1].str());
        }
        if (std::regex_search(line, progressReg)) {
            lastProgressLine = line;
        }
    }
    // 如果有新内容
    if (!lastProgressLine.empty()) {
        std::smatch match;
        // match[0]是整个匹配，match[1]是第一个捕获组。
        if (std::regex_search(lastProgressLine, match, progressReg) && match.size() == 2) {
            return NumberUtil::StringToInt(match[1].str());
        }
    }
    return 0;
}

bool DoubleCheckSuccessInLogFile(std::ifstream &file) {
    // 最后读取一次successfully关键字 进行二次确认
    const std::string successKeyword = "Successfully dump the snapshot to database for devices";
    if (!file.is_open()) {
        Server::ServerLog::Warn("An exception occurred while re-verifying the parsing results; the output log "
                                "file % could not be opened.",
            MemSnapshotParser::Instance().GetParseContext().GetLogPath());
        return false;
    }
    bool successKeywordFound = false;
    std::string line;
    while (std::getline(file, line)) {
        if (line.find(successKeyword) != std::string::npos) {
            successKeywordFound = true;
            break;
        }
    }
    if (!successKeywordFound) {
        Server::ServerLog::Warn(
            "An exception occurred while re-verifying the parsing results: "
            "the success keyword \"%\" was not found in the output log. Please check the log file %.",
            successKeyword, MemSnapshotParser::Instance().GetParseContext().GetLogPath());
        return false;
    }
    return true;
}

void MemSnapshotParser::ParseDaemonTask() {
    Server::ServerLog::Info("[Snapshot] Daemon thread started.");
    std::unordered_set<std::string> notifiedSlices;
    std::ifstream progressFile;
    std::optional<uint64_t> manifestWriteTime;
    while (!Instance().parseContext.IsFinished()) {
        if (Instance().parseContext.GetState() != ParserState::Processing) {
            SLEEP(100);
            continue;
        }
        const auto manifestPath = MemSnapshotSliceService::GetManifestPath(Instance().parseContext.GetPicklePath());
        std::uintmax_t manifestSize = 0;
        uint64_t currentManifestWriteTime = 0;
        if (!Instance().parseContext.GetFileHash().empty() &&
            MemSnapshotSliceService::GetFileFingerprint(manifestPath, manifestSize, currentManifestWriteTime)) {
            const bool manifestChanged =
                !manifestWriteTime.has_value() || manifestWriteTime.value() != currentManifestWriteTime;
            if (manifestChanged) {
                SendReadySliceEvents(notifiedSlices);
                manifestWriteTime = currentManifestWriteTime;
            }
        }
        if (!progressFile.is_open()) {
            const auto logPath = Instance().parseContext.GetLogPath();
            if (FileUtil::IsFilePathExist(logPath)) {
                progressFile = OpenReadFileSafely(logPath);
            }
        }
        if (!progressFile.is_open()) {
            // 解析线程可能并未及时创建出日志文件，因此需要等待
            SLEEP(100);
            continue;
        }
        // 清除上一次读到 EOF 设置的状态，从原文件偏移继续读取追加内容。
        progressFile.clear();
        std::string error = "";
        auto newProgress = ReadProgressInLogFile(progressFile, error);
        if (newProgress < 0 and !error.empty()) {
            Server::ServerLog::Error("Parsing failure information was detected while reading the process logs, and "
                                     "the daemon has exited.");
            Instance().parseContext.SetState(ParserState::FINISH_FAILURE);
            break;
        }
        // 原始事件回放完成后仍有反插、抽样缓存和完整性校验，最终完成前最多展示 99%。
        Instance().parseContext.SetProgress(std::min(newProgress, 99));
        SLEEP(100);
    }
    if (Instance().parseContext.GetState() == ParserState::FINISH_SUCCESS) {
        std::ifstream file = OpenReadFileSafely(Instance().parseContext.GetLogPath());
        const auto manifest = MemSnapshotSliceService::LoadManifest(Instance().parseContext.GetPicklePath());
        if (DoubleCheckSuccessInLogFile(file) && manifest.has_value() && manifest->IsComplete()) {
            Instance().parseContext.SetFileHash(manifest->cacheHash);
            SendReadySliceEvents(notifiedSlices);
            size_t sliceCount = 0;
            for (const auto &[deviceId, device] : manifest->devices) {
                sliceCount += device.slices.size();
            }
            if (notifiedSlices.size() != sliceCount) {
                Instance().parseContext.SetState(ParserState::FINISH_FAILURE);
            } else {
                Instance().parseContext.SetProgress(100);
                Server::ServerLog::Info("Parse thread has successfully finished with double check.");
            }
        } else {
            Server::ServerLog::Warn("The parsing thread returned a success response, but we did not find a "
                                    "corresponding success event confirmed in the logs.");
            Instance().parseContext.SetState(ParserState::FINISH_FAILURE);
        }
    }
    const auto finalState = Instance().parseContext.GetState();
    if (finalState == ParserState::FINISH_FAILURE || !Instance().parseContext.IsInitialSuccessSent() ||
        Instance().parseContext.WasInitialSuccessSentWhileBuilding()) {
        ParseCallBack();
    }
}

void MemSnapshotParser::SendReadySliceEvents(std::unordered_set<std::string> &notifiedSlices) {
    const auto &context = Instance().parseContext;
    const auto manifest = MemSnapshotSliceService::LoadManifest(context.GetPicklePath());
    if (!manifest.has_value()) {
        return;
    }
    bool hasReadySlice = false;
    for (const auto &[deviceId, device] : manifest->devices) {
        for (const auto &slice : device.slices) {
            if (!slice.ready) {
                continue;
            }
            hasReadySlice = true;
            const auto notificationKey = deviceId + ":" + std::to_string(slice.index);
            if (notifiedSlices.find(notificationKey) != notifiedSlices.end()) {
                continue;
            }
            const auto dbPath = MemSnapshotSliceService::ResolveSliceDbPath(context.GetPicklePath(), slice);
            if (dbPath.empty() || !FullDb::MemSnapshotDatabase::BuildMemoryAllocationCache(dbPath, deviceId)) {
                Server::ServerLog::Error(
                    "[Snapshot] Failed to build allocation cache for device %, slice %.", deviceId, slice.index);
                continue;
            }
            notifiedSlices.insert(notificationKey);
            if (!context.IsInitialSuccessSent()) {
                auto initialEvent = Instance().BuildParseSuccessEventFromContext(&notifiedSlices);
                if (initialEvent != nullptr) {
                    const bool parsingComplete = initialEvent->body.snapshotParsingComplete;
                    SendEvent(std::move(initialEvent));
                    Instance().parseContext.MarkInitialSuccessSent(parsingComplete);
                }
                continue;
            }
            auto event = std::make_unique<Protocol::MemSnapshotSliceReadyEvent>();
            event->moduleName = Protocol::MODULE_MEM_SCOPE;
            event->result = true;
            event->body.fileId = context.GetPicklePath();
            event->body.fileHash = context.GetFileHash();
            event->body.deviceId = deviceId;
            event->body.slice = {slice.index, slice.startEventId, slice.endEventId, slice.ready};
            SendEvent(std::move(event));
        }
    }
    if (hasReadySlice && !context.IsInitialSuccessSent()) {
        Server::ServerLog::Warn("[Snapshot] Ready slice exists, but the initial parse event could not be built.");
    }
}

void MemSnapshotParser::ParseCallBack() {
    const std::string filepath = Instance().parseContext.GetPicklePath();
    const auto state = Instance().parseContext.GetState();
    if (state != ParserState::FINISH_SUCCESS && state != ParserState::UP_TO_DATE) {
        const std::string error = StringUtil::FormatString("Failed to parse snapshot data. For details, please check "
                                                           "{}.",
            Instance().parseContext.GetLogPath());
        Server::ServerLog::Error(error);
        auto event = Instance().BuildParseFailEventFromContext(error);
        SendEvent(std::move(event));
        return;
    }
    auto event = Instance().BuildParseSuccessEventFromContext();
    if (event == nullptr) {
        const std::string error =
            StringUtil::FormatString("Failed to build success event for snapshot data {}.", filepath);
        Server::ServerLog::Error(error);
        auto failedEvent = Instance().BuildParseFailEventFromContext(error);
        SendEvent(std::move(failedEvent));
        return;
    }
    SendEvent(std::move(event));
}

std::unique_ptr<MemScopeParseSuccessEvent> MemSnapshotParser::BuildParseSuccessEventFromContext(
    const std::unordered_set<std::string> *preparedSlices) const {
    const auto manifest = MemSnapshotSliceService::LoadManifest(parseContext.GetPicklePath());
    if (!manifest.has_value()) {
        Server::ServerLog::Error("[Snapshot] Failed to build success event: load slice manifest failed.");
        return nullptr;
    }
    auto event = std::make_unique<Protocol::MemScopeParseSuccessEvent>();
    event->moduleName = Protocol::MODULE_MEM_SCOPE; // moduleName设置为memscope以复用MemScope的事件/请求/响应路由
    event->result = true;
    Protocol::MemScopeParseSuccessEventBody body;
    body.fileId = parseContext.GetPicklePath();
    body.fileHash = parseContext.GetFileHash();
    body.snapshotParsingComplete = manifest->IsComplete();
    std::vector<std::string> devices;
    for (const auto &[deviceId, device] : manifest->devices) {
        Protocol::MemSnapshotDeviceSliceEventInfo deviceEvent;
        deviceEvent.eventCount = device.eventCount;
        deviceEvent.sliceCount = device.sliceCount;
        for (const auto &slice : device.slices) {
            const auto notificationKey = deviceId + ":" + std::to_string(slice.index);
            const bool ready = slice.ready &&
                (preparedSlices == nullptr || preparedSlices->find(notificationKey) != preparedSlices->end());
            deviceEvent.slices.push_back({slice.index, slice.startEventId, slice.endEventId, ready});
            if (ready) {
                deviceEvent.readySlices.emplace_back(slice.index);
            }
        }
        body.snapshotSlices.emplace(deviceId, std::move(deviceEvent));
        if (!body.snapshotSlices.at(deviceId).readySlices.empty()) {
            devices.emplace_back(deviceId);
            body.deviceIds[deviceId] = {"BLOCK"};
        }
    }
    Server::ServerLog::Info("[Snapshot] Recognized devices: %", StringUtil::join(devices, ", "));
    body.module = Protocol::MODULE_MEM_SNAPSHOT; // body.module设置为真实数据类型以适配前端区分模块类型
    event->body = body;
    return event;
}

std::unique_ptr<ParseFailEvent> MemSnapshotParser::BuildParseFailEventFromContext(const std::string &errMsg) const {
    auto event = std::make_unique<ParseFailEvent>();
    event->moduleName = Protocol::MODULE_TIMELINE;
    event->result = false;
    event->body.rankId = parseContext.GetPicklePath();
    event->body.error = errMsg;
    event->body.dbPath = parseContext.GetOutputDbPath();
    return event;
}
} // namespace Dic::Module
