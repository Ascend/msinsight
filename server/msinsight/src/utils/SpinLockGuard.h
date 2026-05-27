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

#ifndef PROFILER_SERVER_SPINLOCKGARD_H
#define PROFILER_SERVER_SPINLOCKGARD_H
#include <atomic>

namespace Dic {
class SpinLock {
  public:
    SpinLock() : flag(false) {}

    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // 自旋等待，直到锁被释放
        }
    }

    void unlock() { flag.clear(std::memory_order_release); }

  private:
    std::atomic_flag flag;
};

class SpinLockGuard {
  public:
    explicit SpinLockGuard(SpinLock &spinLock) : spinLock(spinLock) { spinLock.lock(); }

    ~SpinLockGuard() { spinLock.unlock(); }

    // 禁用复制和赋值
    SpinLockGuard(const SpinLockGuard &) = delete;
    SpinLockGuard &operator=(const SpinLockGuard &) = delete;

  private:
    SpinLock &spinLock;
};
}

#endif // PROFILER_SERVER_SPINLOCKGARD_H
