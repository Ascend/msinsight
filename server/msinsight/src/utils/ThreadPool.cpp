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

#include "ThreadPool.h"
#include "ServerLog.h"

namespace Dic {
using namespace Dic::Server;
void ThreadPool::ThreadFunc(ThreadPool &threadPool, int index) {
    bool hasTask = false;
    while (threadPool.running) {
        std::function<void()> func;
        {
            std::unique_lock<std::mutex> lock(threadPool.taskMutex);
            if (threadPool.taskQueue.Empty()) {
                threadPool.taskCv.wait(
                    lock, [&threadPool]() { return !threadPool.running || !threadPool.taskQueue.Empty(); });
            }
            hasTask = threadPool.taskQueue.Pop(func);
        }
        if (hasTask) {
            threadPool.runningTasks++;
            func();
            threadPool.runningTasks--;
        }
        if (threadPool.waiting && threadPool.runningTasks == 0 && threadPool.taskQueue.Empty()) {
            threadPool.taskDoneCv.notify_all();
        }
    }
    ServerLog::Info("[Thread worker] worker ", index, " exit.");
}

ThreadPool::ThreadPool(uint32_t threadCount) {
    ServerLog::Info("[Thread pool] Init. thread count:", threadCount);
    for (uint32_t i = 0; i < threadCount; i++) {
        threads.emplace_back(ThreadPool::ThreadFunc, std::ref(*this), i);
    }
}

ThreadPool::~ThreadPool() { ShutDown(); }

void ThreadPool::Reset() {
    ServerLog::Info("[Thread pool] Reset.");
    taskQueue.Clear();
    WaitForAllTasks();
    ServerLog::Info("[Thread pool] Reset end.");
}

void ThreadPool::ShutDown() {
    ServerLog::Info("[Thread pool] Shut down.");
    if (running) {
        WaitForAllTasks();
        taskQueue.Clear();
        running = false;
        taskCv.notify_all();
        for (auto &thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        threads.clear();
    }
    ServerLog::Info("[Thread pool] Shut down end.");
}

void ThreadPool::WaitForAllTasks() {
    ServerLog::Info("[Thread pool] Wait for all tasks to complete.");
    std::unique_lock<std::mutex> lock(taskMutex);
    waiting = true;
    taskDoneCv.wait(lock, [this]() { return runningTasks == 0 && taskQueue.Empty(); });
    waiting = false;
    ServerLog::Info("[Thread pool] All tasks completed.");
}
ThreadPool &ThreadPool::Instance() {
    auto maxThreadCount = std::thread::hardware_concurrency();
    static ThreadPool pool(maxThreadCount);
    return pool;
}
}
