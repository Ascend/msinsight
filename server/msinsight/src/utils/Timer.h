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
#include <string>
#include <chrono>
#include "ServerLog.h"
// 性能测试工具类，利用生命周期进行测试
namespace Dic {
class Timer {
  public:
    Timer(const std::string &name) : m_Name(name), m_Stopped(false) {
        m_StartTimepoint = std::chrono::high_resolution_clock::now();
    }

    void Stop() {
        auto endTimepoint = std::chrono::high_resolution_clock::now();
        long long start =
            std::chrono::time_point_cast<std::chrono::milliseconds>(m_StartTimepoint).time_since_epoch().count();
        long long end =
            std::chrono::time_point_cast<std::chrono::milliseconds>(endTimepoint).time_since_epoch().count();
        Server::ServerLog::Info("Performance : ", m_Name, ": ", end - start, "ms");
        m_Stopped = true;
    }

    ~Timer() {
        if (!m_Stopped) {
            Stop();
        }
    }

  private:
    std::string m_Name;
    bool m_Stopped;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;
};
}
