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

#include "ServerLog.h"
#include "TraceIdManager.h"
#include "LogUtil.h"

namespace Dic::Server {
static std::unique_ptr<LogUtil> printInstance = nullptr;
static std::unique_ptr<LogUtil> recordInstance = nullptr;
std::string ServerLog::currentLogPath;
std::mutex ServerLog::currentLogPathMutex;
#ifdef _WIN32
const std::string_view PATH_SLASH = "\\";
#else
const std::string_view PATH_SLASH = "/";
#endif

void ServerLog::Initialize(
    const std::string &logPath, const int &logSize, const std::string &logLevelStr, std::string wsPortStr) {
    ServerLog &serverLog = Instance();
    serverLog.wsPort = wsPortStr;
    if (printInstance == nullptr) {
        printInstance = std::make_unique<LogUtil>(LogOutType::TERMINAL, "", wsPortStr);
    }
    if (recordInstance == nullptr) {
        std::vector<std::string> paths = {logPath, "profiler_server.log"};
        std::string logFileName = StringUtil::join(paths, PATH_SLASH);
        LogLevel logLevel = LogUtil::GetLogLevel(logLevelStr);
        recordInstance = std::make_unique<LogUtil>(LogOutType::FILE, logFileName, wsPortStr, logSize);
        recordInstance->SetLogLevel(logLevel);
    }
}
void ServerLog::Initialize(const LogLevel &level) {
    if (recordInstance == nullptr) {
        std::vector<std::string> paths = {".", "profiler_server.log"};
        std::string logPath = StringUtil::join(paths, PATH_SLASH);
        const int logSize = 32 * 1024 * 1024;
        auto instance = std::make_unique<LogUtil>(LogOutType::FILE, logPath, wsPort);
        instance->SetLogLevel(level).SetMaxSize(logSize);
        recordInstance = std::move(instance);
    }
    if (printInstance == nullptr) {
        printInstance = std::make_unique<LogUtil>(LogOutType::TERMINAL, "", wsPort);
    }
}

std::string ServerLog::GetCurrentLogPath() {
    std::lock_guard<std::mutex> lock(ServerLog::currentLogPathMutex);
    return ServerLog::currentLogPath;
}

void ServerLog::SetCurrentLogPath(const std::string &path) {
    std::lock_guard<std::mutex> lock(ServerLog::currentLogPathMutex);
    ServerLog::currentLogPath = path;
}

const std::string ServerLog::GetLogHead(const LogLevel &level) {
    std::string head;
    std::string traceId = TraceIdManager::GetTraceId();
    head.append(LogPrefix::Instance().TimePrefix()).append("|").append(LogPrefix::Instance().LevelPrefix(level));
    if (!traceId.empty()) {
        head.append("[TraceId=").append(traceId).append("]");
    }
    head.append(" ");
    return head;
}
void ServerLog::RecordImpl(const LogLevel &level, std::vector<std::string> &logStrList) {
    recordInstance->LogT(level, GetLogHead(level), logStrList);
    std::string path = recordInstance->GetLogFilePath();
    SetCurrentLogPath(path);
}

void ServerLog::PrintImpl(const LogLevel &level, std::vector<std::string> &logStrList) {
    printInstance->LogT(level, GetLogHead(level), logStrList);
}
ServerLog &ServerLog::Instance() {
    static ServerLog instance;
    return instance;
}
}
