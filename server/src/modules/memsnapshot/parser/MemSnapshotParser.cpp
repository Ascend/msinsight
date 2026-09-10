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
#include "DataBaseManager.h"
#include "HashUtil.h"
#include "MemSnapshotParser.h"
#include "MemSnapshotDatabase.h"
#include "MemSnapshotSliceService.h"
#include "WsSender.h"

#include <chrono>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#define SLEEP(ms) Sleep(ms)
#else
#define SLEEP(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#endif

namespace Dic::Module {
using namespace Dic::Module::Timeline;
constexpr std::string_view MEM_SNAPSHOT_PARSER_HASH_SALT = "mem_snapshot_parser_v2";
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

std::string MemSnapshotParserContext::GetFileHash() const { return fileHash; }

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

std::string MemSnapshotParser::CalculateFileHash(const std::string &filePath) {
    const std::string hash = HashUtil::CalculateFileSha256(filePath, MEM_SNAPSHOT_PARSER_HASH_SALT);
    if (hash.empty()) {
        Server::ServerLog::Error("[Snapshot] Failed to open pickle file for hashing: %.", filePath);
    }
    return hash;
}

void MemSnapshotParser::AsyncParseMemSnapshotPickle(const std::string &pickleFilePath) {
    const std::string outputPath = MemSnapshotSliceService::GetArtifactDirectory(pickleFilePath);
    const std::string logPath = MemSnapshotSliceService::GetLogPath(pickleFilePath);
    parseContext.Reset(pickleFilePath, logPath, outputPath, CalculateFileHash(pickleFilePath));
    auto traceId = TraceIdManager::GenerateTraceId();
    Server::ServerLog::Info("[Snapshot] Parsing pickle file: %, log file: %, output db file: %.",
        parseContext.GetPicklePath(), parseContext.GetLogPath(), parseContext.GetOutputDbPath());
    _threadPool->AddTask(ParseMemSnapshotTask, traceId);
    _threadPool->AddTask(ParseDaemonTask, traceId);
}

MemSnapshotParserContext &MemSnapshotParser::GetParseContext() { return parseContext; }

/***
 * 在满足以下任意条件之一（按顺序检查）时，需要重新解析pickle文件：
 * 1. 数据库连接打开失败或db初始化失败
 * 2. 通过2的校验，但数据库版本与当前版本不一致
 *
 * @brief 检查是否需要解析或重新解析pickle文件
 * @return true 需要解析或重新解析，此时会关闭已打开的连接并清空DatabaseManager纳管实例。
 * @return false 不需要解析或重新解析。此时将不会清空DatabaseManager及纳管实例，可以不需要重复打开。
 */
bool MemSnapshotParser::CheckIfParsingNeed(const MemSnapshotParserContext &context) {
    const auto manifest = MemSnapshotSliceService::LoadManifest(context.GetPicklePath());
    if (!manifest.has_value() || !manifest->IsComplete()) {
        Server::ServerLog::Info("[Snapshot] Slice manifest is missing or incomplete; the file needs to re-parse.");
        MemSnapshotDatabase::Reset();
        return true;
    }
    const std::string snapshotCacheHash = context.GetFileHash();
    const std::string &cachedSnapshotHash = manifest->cacheHash;
    if (snapshotCacheHash.empty() || cachedSnapshotHash.empty() || snapshotCacheHash != cachedSnapshotHash) {
        Server::ServerLog::Info(
            "[Snapshot] Snapshot cache hash changed or is unavailable. The file needs to re-parse.");
        MemSnapshotDatabase::Reset();
        return true;
    }
    for (const auto &[deviceId, device] : manifest->devices) {
        for (const auto &slice : device.slices) {
            const auto dbPath = MemSnapshotSliceService::ResolveSliceDbPath(context.GetPicklePath(), slice);
            if (!slice.ready || dbPath.empty() || !FileUtil::CheckFilePathExist(dbPath)) {
                Server::ServerLog::Info(
                    "[Snapshot] Slice artifact is missing for device %, slice %; the file needs to re-parse.", deviceId,
                    slice.index);
                MemSnapshotDatabase::Reset();
                return true;
            }
            std::recursive_mutex validationMutex;
            FullDb::MemSnapshotDatabase validationDatabase(validationMutex);
            if (!validationDatabase.OpenDbReadOnly(dbPath) || !validationDatabase.IsDeviceIdValid(deviceId)) {
                Server::ServerLog::Info(
                    "[Snapshot] Slice database is incomplete for device %, slice %; the file needs to re-parse.",
                    deviceId, slice.index);
                MemSnapshotDatabase::Reset();
                return true;
            }
            if (!FullDb::MemSnapshotDatabase::HasMemoryAllocationCache(dbPath, deviceId) &&
                !FullDb::MemSnapshotDatabase::BuildMemoryAllocationCache(dbPath, deviceId)) {
                Server::ServerLog::Info(
                    "[Snapshot] Allocation cache is missing for device %, slice %; the file needs to re-parse.",
                    deviceId, slice.index);
                MemSnapshotDatabase::Reset();
                return true;
            }
        }
    }
    return false;
}

MemSnapshotParser::MemSnapshotParser() {
    // MemSnapshot解析snapshot解析至少需要双线程
    _threadPool = std::make_unique<ThreadPool>(2);
}

MemSnapshotParser::~MemSnapshotParser() { _threadPool->ShutDown(); }

void MemSnapshotParser::ParseMemSnapshotTask() {
    Server::ServerLog::Info("[Snapshot] Parse snapshot thread started.");
    if (!CheckIfParsingNeed(Instance().parseContext)) {
        Server::ServerLog::Info("[Snapshot] Parsing pickle file: %, artifact directory: % is up-to-date.",
            Instance().parseContext.GetPicklePath(), Instance().parseContext.GetOutputDbPath());
        Instance().parseContext.SetProgress(100);
        Instance().parseContext.SetState(ParserState::UP_TO_DATE);
        return;
    }
    // 需要首次解析或重新解析的场景
    const std::vector<std::string> staleStateFiles = {
        Instance().parseContext.GetLogPath(),
        MemSnapshotSliceService::GetManifestPath(Instance().parseContext.GetPicklePath()),
    };
    for (const auto &path : staleStateFiles) {
        if (FileUtil::CheckFilePathExist(path) && !FileUtil::RemoveFile(path)) {
            Server::ServerLog::Warn("[Snapshot] Failed to remove stale parsing state file: %.", path);
        }
    }
    const std::string memSnapDumpScriptsPath = FileUtil::SplicePath("mem_snap_dump", "tools", "dump2db.py");
    Server::ServerLog::Info("[Snapshot] Start parsing.");
    std::vector<std::string> arguments{Instance().parseContext.GetPicklePath(), "--dump_dir",
        Instance().parseContext.GetOutputDbPath(), "--log", Instance().parseContext.GetLogPath(), "--cache_hash",
        Instance().parseContext.GetFileHash(), "--events_per_slice", std::to_string(MEM_SNAPSHOT_EVENTS_PER_SLICE)};
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
    std::optional<fs::file_time_type> manifestWriteTime;
    while (!Instance().parseContext.IsFinished()) {
        if (Instance().parseContext.GetState() != ParserState::Processing) {
            SLEEP(100);
            continue;
        }
        const auto manifestPath = MemSnapshotSliceService::GetManifestPath(Instance().parseContext.GetPicklePath());
        std::error_code manifestError;
        const auto currentManifestWriteTime = fs::last_write_time(manifestPath, manifestError);
        const bool manifestChanged =
            !manifestWriteTime.has_value() || manifestWriteTime.value() != currentManifestWriteTime;
        if (!manifestError && manifestChanged) {
            SendReadySliceEvents(notifiedSlices);
            manifestWriteTime = currentManifestWriteTime;
        }
        if (!progressFile.is_open()) {
            progressFile.open(Instance().parseContext.GetLogPath());
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
    SendReadySliceEvents(notifiedSlices);
    if (Instance().parseContext.GetState() == ParserState::FINISH_SUCCESS) {
        std::ifstream file(Instance().parseContext.GetLogPath());
        if (DoubleCheckSuccessInLogFile(file) && Instance().TryOpenParsingResultDbAndSetVersion()) {
            Instance().parseContext.SetProgress(100);
            Server::ServerLog::Info("Parse thread has successfully finished with double check.");
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

bool MemSnapshotParser::TryOpenParsingResultDbAndSetVersion() const { return !CheckIfParsingNeed(parseContext); }

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
