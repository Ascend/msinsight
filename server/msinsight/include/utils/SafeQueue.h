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

#ifndef THREADPOOL_SAFE_QUEUE_H
#define THREADPOOL_SAFE_QUEUE_H
#include <deque>
#include <mutex>

namespace Dic {
template <typename T> class SafeQueue {
  public:
    void Push(T &t) {
        std::unique_lock<std::mutex> lock(mutex);
        deque.emplace_back(std::forward<T>(t));
    }

    bool Pop(T &t) {
        std::unique_lock<std::mutex> lock(mutex);
        if (deque.empty()) {
            return false;
        }
        t = std::move(deque.front());
        deque.pop_front();
        return true;
    }

    bool Empty() {
        std::unique_lock<std::mutex> lock(mutex);
        return deque.empty();
    }

    size_t Size() {
        std::unique_lock<std::mutex> lock(mutex);
        return deque.size();
    }

    void Clear() {
        std::unique_lock<std::mutex> lock(mutex);
        deque.clear();
    }

    SafeQueue &operator<<(T t) {
        std::unique_lock<std::mutex> lock(mutex);
        if (t != nullptr) {
            deque.emplace_back(std::move(t));
        }
        return *this;
    }

  private:
    std::deque<T> deque;
    std::mutex mutex;
};
}

#endif // THREADPOOL_SAFE_QUEUE_H
