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
#include "IEConnectionPool.h"
#include <utility>
#include "SystemUtil.h"

namespace Dic::Module::IE {
IEConnectionPool::IEConnectionPool(std::string dbPath, std::recursive_mutex &insertSqlMutex)
    : path(std::move(dbPath)), insertSqlMutex(insertSqlMutex) {}

IEConnectionPool::~IEConnectionPool() {
    try {
        Stop();
    } catch (const std::exception &) {
        // do nothing
    }
}

std::shared_ptr<Database> IEConnectionPool::GetConnection() {
    std::unique_lock<std::mutex> lock(mutex);
    if (!valid) {
        return nullptr;
    }
    Database *conn = nullptr;
    if (!idlePool.empty()) {
        conn = idlePool.front();
        idlePool.pop_front();
    } else if (activePool.size() < maxActiveConnections) {
        conn = CreatConnection();
    } else {
        ServerLog::Info("Wait idle connection.");
        cv.wait_for(lock, std::chrono::seconds(maxWaitTime));
        if (valid && !idlePool.empty()) {
            conn = idlePool.front();
            idlePool.pop_front();
        }
    }
    if (conn == nullptr) {
        ServerLog::Error("Get connection Failed.");
        return nullptr;
    }
    std::shared_ptr<Database> connPtr(conn, [this](Database *conn) { ReleaseConnection(conn); });
    return connPtr;
}

/**
 * Set max active connections.
 *
 * @param count max active connections.
 */
void IEConnectionPool::SetMaxActiveCount() {
    const static unsigned int MAX_COUNT = 10;
    const static unsigned int CPU_CORE_COUNT = std::min(SystemUtil::GetCpuCoreCount(), MAX_COUNT);
    maxActiveConnections = std::max(maxActiveConnections, CPU_CORE_COUNT);
}

void IEConnectionPool::Stop() {
    std::unique_lock<std::mutex> lock(mutex);
    if (!valid) {
        return;
    }
    valid = false;
    ServerLog::Info("Wait all connection released. path:", path, ", idle size:", idlePool.size(),
        ", active size:", activePool.size());
    while (idlePool.size() != activePool.size()) {
        cv.wait_for(lock, std::chrono::seconds(maxWaitTime));
    }
    ServerLog::Info("All connection released.");
    for (auto conn : activePool) {
        delete conn;
    }
    idlePool.clear();
    activePool.clear();
    path = "";
}

void IEConnectionPool::ReleaseConnection(Database *conn) {
    std::unique_lock<std::mutex> lock(mutex);
    idlePool.emplace_back(conn);
    if (valid) {
        cv.notify_one();
    } else {
        ServerLog::Info("Release connection. idle pool size:", idlePool.size());
        cv.notify_all();
    }
}

Database *IEConnectionPool::CreatConnection() {
    int retryCount = 0;
    while (retryCount < maxRetryAttempts) {
        auto *conn = new Database(insertSqlMutex);
        if (!conn->OpenDb(path, false)) {
            delete conn;
            retryCount++;
            continue;
        }
        activePool.emplace_back(conn);
        return conn;
    }
    ServerLog::Error("Failed create connect. path:", path);
    return nullptr;
}

std::string IEConnectionPool::GetDbPath() { return path; }
} // namespace Dic::Module::IE
// end of namespace Module
// end of namespace Dic
