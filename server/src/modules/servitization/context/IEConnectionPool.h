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

#ifndef PROFILER_SERVER_IECONNECTION_POOL_H
#define PROFILER_SERVER_IECONNECTION_POOL_H

#include <string>
#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <utility>
#include "Database.h"
#include "ServerLog.h"
namespace Dic::Module::IE {
using namespace Dic::Server;
class IEConnectionPool {
  public:
    explicit IEConnectionPool(std::string dbPath, std::recursive_mutex &insertSqlMutex);
    ~IEConnectionPool();
    IEConnectionPool(const IEConnectionPool &) = delete;
    IEConnectionPool &operator=(const IEConnectionPool &) = delete;
    IEConnectionPool(IEConnectionPool &&) = delete;
    IEConnectionPool &operator=(IEConnectionPool &&) = delete;
    std::shared_ptr<Database> GetConnection();
    void SetMaxActiveCount();
    std::string GetDbPath();
    void Stop();

  private:
    std::function<Database *()> databaseCreateCall;
    std::mutex mutex;
    std::string path;
    std::recursive_mutex &insertSqlMutex;
    std::condition_variable cv;
    bool valid = true;
    unsigned int maxActiveConnections = 20;
    int maxRetryAttempts = 3;
    int maxWaitTime = 2; // seconds
    std::deque<Database *> idlePool;
    std::deque<Database *> activePool;

    Database *CreatConnection();
    void ReleaseConnection(Database *conn);
};
} // namespace Dic::Module::IE
// end of namespace Module
// end of namespace Dic

#endif // PROFILER_SERVER_IECONNECTION_POOL_H
