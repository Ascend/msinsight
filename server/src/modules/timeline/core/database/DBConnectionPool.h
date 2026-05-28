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

#ifndef PROFILER_SERVER_DBCONNECTIONPOOL_H
#define PROFILER_SERVER_DBCONNECTIONPOOL_H

#include <condition_variable>
#include <functional>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include "ServerLog.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;
template <typename T> class DBConnectionPool {
  public:
    explicit DBConnectionPool(std::string dbPath, std::function<T *()> call);

    ~DBConnectionPool();

    DBConnectionPool(const DBConnectionPool &) = delete;

    DBConnectionPool &operator=(const DBConnectionPool &) = delete;

    DBConnectionPool(DBConnectionPool &&) = delete;

    DBConnectionPool &operator=(DBConnectionPool &&) = delete;

    std::shared_ptr<T> GetConnection();

    void SetMaxActiveCount(unsigned int count);

    std::string GetDbPath() const;

    void Stop();

  private:
    std::function<T *()> databaseCreateCall;
    std::mutex mutex;
    std::condition_variable cv;
    std::string path;
    bool valid = true;
    unsigned int maxActiveConnections = 30;
    int maxRetryAttempts = 3;
    int maxWaitTime = 2; // seconds
    std::deque<T *> idlePool;
    std::deque<T *> activePool;

    T *CreatConnection();

    void ReleaseConnection(T *conn);
};

template <typename T>
DBConnectionPool<T>::DBConnectionPool(std::string dbPath, std::function<T *()> call)
    : databaseCreateCall(std::move(call)), path(std::move(dbPath)) {}

template <typename T> DBConnectionPool<T>::~DBConnectionPool() {
    try {
        Stop();
    } catch (const std::exception &) {
        // do nothing
    }
}

template <typename T> std::shared_ptr<T> DBConnectionPool<T>::GetConnection() {
    std::unique_lock<std::mutex> lock(mutex);
    if (!valid) {
        return nullptr;
    }
    T *conn = nullptr;
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
    std::shared_ptr<T> connPtr(conn, [this](T *conn) { ReleaseConnection(conn); });
    return connPtr;
}

template <typename T> void DBConnectionPool<T>::SetMaxActiveCount(unsigned int count) {
    maxActiveConnections = std::max(maxActiveConnections, count);
}

template <typename T> std::string DBConnectionPool<T>::GetDbPath() const { return path; }

template <typename T> void DBConnectionPool<T>::Stop() {
    std::unique_lock<std::mutex> lock(mutex);
    if (!valid) {
        return;
    }
    valid = false;
    while (idlePool.size() != activePool.size()) {
        cv.wait_for(lock, std::chrono::seconds(maxWaitTime));
    }
    for (auto conn : activePool) {
        delete conn;
    }
    idlePool.clear();
    activePool.clear();
    path = "";
}

template <typename T> T *DBConnectionPool<T>::CreatConnection() {
    int retryCount = 0;
    while (retryCount < maxRetryAttempts) {
        T *conn = databaseCreateCall();
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

template <typename T> void DBConnectionPool<T>::ReleaseConnection(T *conn) {
    std::unique_lock<std::mutex> lock(mutex);
    idlePool.emplace_back(conn);
    if (valid) {
        cv.notify_one();
    } else {
        ServerLog::Info("Release connection. idle pool size:", idlePool.size());
        cv.notify_all();
    }
}
} // Timeline::Module::Dic

#endif // PROFILER_SERVER_DBCONNECTIONPOOL_H
