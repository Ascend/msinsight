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
#include "MemSnapshotParser.h"
#include "WsSender.h"

#ifdef _WIN32
#include <windows.h>
#define SLEEP(ms) Sleep(ms)
#else
#define SLEEP(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#endif

namespace Dic::Module {
using namespace Dic::Module::Timeline;
void MemSnapshotParserContext::Reset(std::string nPicklePath, std::string nLogPath, std::string nOutputPath) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    picklePath = std::move(nPicklePath);
    logPath = std::move(nLogPath);
    outputDbPath = std::move(nOutputPath);
    state = ParserState::INIT;
    progress = 0;
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
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (progress == newProgress) {
        return;
    }
    progress = newProgress;
    Server::ServerLog::Info("Snapshot pickle file parse progress changed: ", static_cast<int>(progress));
    SendParseProgressEvent(progress);
}

void MemSnapshotParserContext::SendParseProgressEvent(int progress) {
    static auto lastSendTime = std::chrono::steady_clock::time_point();
    static const std::chrono::milliseconds minInterval(2000);
    auto now = std::chrono::steady_clock::now();
    if (lastSendTime.time_since_epoch().count() != 0 && now - lastSendTime < minInterval) {
        return;
    }
    lastSendTime = now;
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
    const std::string filename = FileUtil::GetFileName(pickleFilePath);
    const auto timeT = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::stringstream timeStr;
    timeStr << std::put_time(std::localtime(&timeT), "%Y_%m_%d_%H_%M_%S");
    const std::string outputDbPath = StringUtil::StrJoin(pickleFilePath, ".db");
    const std::string logName = StringUtil::FormatString("{}_{}.log", FileUtil::StemFile(filename), timeStr.str());
    const std::string logPath = FileUtil::SplicePath(FileUtil::GetParentPath(pickleFilePath), logName);
    parseContext.Reset(pickleFilePath, logPath, outputDbPath);
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
    auto snapshotDb = DataBaseManager::Instance().GetMemSnapshotDatabase(context.GetPicklePath());
    if (snapshotDb == nullptr) {
        Server::ServerLog::Warn(
            "[Snapshot] Cannot get database connection by fileId: %, trying to re-parse.", context.GetPicklePath());
        return true;
    }
    // 可能为首次打开db
    if (!snapshotDb->IsOpen() && !snapshotDb->OpenDbReadOnly(context.GetOutputDbPath())) {
        Server::ServerLog::Warn(
            "[Snapshot] Cannot open database file: %, trying to re-parse.", context.GetOutputDbPath());
        // 此处返回需要重新解析时，必须关闭数据库连接并清空db缓存，否则可能导致后续解析完成后，从DataBaseManager获取到旧的db连接而未正确重新初始化
        MemSnapshotDatabase::Reset();
        return true;
    }
    if (snapshotDb->IsDatabaseVersionChange()) {
        Server::ServerLog::Info("[Snapshot] Database version changed. The pickle file need to re-parse.");
        // 同上述，必须关闭数据库连接并清空db缓存
        MemSnapshotDatabase::Reset();
        return true;
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
    const auto dbPath = Instance().parseContext.GetOutputDbPath();
    // 如果解析db已存在
    if (FileUtil::CheckFilePathExist(dbPath)) {
        // 检查db是否可写，为后续重新解析做准备
        if (!FileUtil::CheckPathSecurity(dbPath, CHECK_FILE_WRITE)) {
            Server::ServerLog::Error("[Snapshot] Existing output db file: % "
                                     "is not writable, parse interrupted. ",
                dbPath);
            Instance().parseContext.SetState(ParserState::FINISH_FAILURE);
            return;
        }
        // 通过存在性及权限检查，判断是否需要重新解析
        if (!CheckIfParsingNeed(Instance().parseContext)) {
            // 不需要重新解析的场景，可以直接发送解析成功事件，且在上述CheckIf方法中已经打开db并纳管到了databaseManager中
            Server::ServerLog::Info("[Snapshot] Parsing pickle file: %, output db file: % is up-to-date.",
                Instance().parseContext.GetPicklePath(), Instance().parseContext.GetOutputDbPath());
            Instance().parseContext.SetProgress(100);
            Instance().parseContext.SetState(ParserState::UP_TO_DATE);
            return;
        }
    }
    // 需要首次解析或重新解析的场景
    const std::string memSnapDumpScriptsPath = FileUtil::SplicePath("mem_snap_dump", "tools", "dump2db.py");
    Server::ServerLog::Info("[Snapshot] Start parsing.");
    std::vector<std::string> arguments{
        Instance().parseContext.GetPicklePath(), "--log", Instance().parseContext.GetLogPath()};
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
    const std::regex progressReg(R"((\d+(?:\.\d+)?)% of entries have been processed)");
    int checkIntervalMs = 100;
    while (!Instance().parseContext.IsFinished()) {
        std::ifstream file(Instance().parseContext.GetLogPath());
        if (!file.is_open()) {
            // 解析线程可能并未及时创建出日志文件，因此需要等待
            SLEEP(500);
            continue;
        }
        std::string error = "";
        auto newProgress = ReadProgressInLogFile(file, error);
        if (newProgress < 0 and !error.empty()) {
            Server::ServerLog::Error("Parsing failure information was detected while reading the process logs, and "
                                     "the daemon has exited.");
            Instance().parseContext.SetState(ParserState::FINISH_FAILURE);
            break;
        }
        Instance().parseContext.SetProgress(newProgress);
        SLEEP(checkIntervalMs); // 从100ms开始2倍增长，最大5s
        checkIntervalMs = std::min(checkIntervalMs * 2, 5000);
    }
    if (Instance().parseContext.GetState() == ParserState::FINISH_SUCCESS) {
        std::ifstream file(Instance().parseContext.GetLogPath());
        if (DoubleCheckSuccessInLogFile(file) && Instance().TryOpenParsingResultDbAndSetVersion()) {
            Server::ServerLog::Info("Parse thread has successfully finished with double check.");
        } else {
            Server::ServerLog::Warn("The parsing thread returned a success response, but we did not find a "
                                    "corresponding success event confirmed in the logs.");
        }
    }
    ParseCallBack();
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

bool MemSnapshotParser::TryOpenParsingResultDbAndSetVersion() const {
    const std::string dbPath = parseContext.GetOutputDbPath();
    if (!FileUtil::CheckPathSecurity(dbPath, CHECK_FILE_READ)) {
        Server::ServerLog::Error("[Snapshot] Double Check failed to verify the validity of the result database and "
                                 "establish a connection.");
        return false;
    }
    // 从DatabaseManager中获取MemSnapshotDatabase时，应使用原文件路径作为fileId而不是解析后的Db路径
    const auto snapshotDb = DataBaseManager::Instance().GetMemSnapshotDatabase(parseContext.GetPicklePath());
    if (!snapshotDb) {
        Server::ServerLog::Error("[Snapshot] Double Check failed to get the snapshot database.");
        return false;
    }
    if (!snapshotDb->IsOpen() && !snapshotDb->OpenDbReadOnly(dbPath)) {
        Server::ServerLog::Warn("[Snapshot] Double Check failed to open database file: %.", dbPath);
        return false;
    }
    // 缺省目标版本将设置为当前profiler_server编译时间
    return snapshotDb->SetDataBaseVersion();
}

std::unique_ptr<MemScopeParseSuccessEvent> MemSnapshotParser::BuildParseSuccessEventFromContext() const {
    // 从DatabaseManager中获取MemSnapshotDatabase时，应使用原文件路径作为fileId而不是解析后的Db路径
    const auto snapshotDb = DataBaseManager::Instance().GetMemSnapshotDatabase(parseContext.GetPicklePath());
    if (!snapshotDb) {
        Server::ServerLog::Error("[Snapshot] Failed to build success event: get the snapshot database failed.");
        return nullptr;
    }
    if (!snapshotDb->IsOpen() && !snapshotDb->OpenDbReadOnly(parseContext.GetOutputDbPath())) {
        Server::ServerLog::Error(
            "[Snapshot] Failed to build success event: open database file failed: %.", parseContext.GetOutputDbPath());
        return nullptr;
    }
    auto event = std::make_unique<Protocol::MemScopeParseSuccessEvent>();
    event->moduleName = Protocol::MODULE_MEM_SCOPE; // moduleName设置为memscope以复用MemScope的事件/请求/响应路由
    event->result = true;
    Protocol::MemScopeParseSuccessEventBody body;
    body.fileId = parseContext.GetPicklePath();
    auto const devices = snapshotDb->GetDeviceIds();
    Server::ServerLog::Info("[Snapshot] Reconized devices: %", StringUtil::join(devices, ", "));
    for (const auto &deviceId : devices) {
        body.deviceIds[deviceId] = {"BLOCK"};
    }
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
