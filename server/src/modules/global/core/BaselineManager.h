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

#ifndef PROFILER_SERVER_BASELINEMANAGER_H
#define PROFILER_SERVER_BASELINEMANAGER_H

#include <string>
#include <vector>
#include "shared_mutex"
#include "SystemMemoryDatabaseDef.h"

namespace Dic {
namespace Module {
namespace Global {
class BaselineManager {
  public:
    static BaselineManager &Instance();
    BaselineManager(const BaselineManager &) = delete;
    BaselineManager &operator=(const BaselineManager &) = delete;
    BaselineManager(BaselineManager &&) = delete;
    BaselineManager &operator=(BaselineManager &&) = delete;
    std::string GetBaselineId();
    bool IsBaselineRankId(const std::string &rankId);
    void SetBaselineInfo(const BaselineInfo &baselineInfo);
    std::string GetCompareClusterPath() { return compareClusterPath; }

    void SetCompareClusterPath(const std::string &clusterPath) { compareClusterPath = clusterPath; }
    void SetBaselineClusterPath(const std::string &clusterPath);
    std::string GetBaseLineClusterPath();
    void Reset();

  private:
    BaselineManager() = default;
    ~BaselineManager() = default;
    std::string baselineRankId;
    std::string baselineHost;
    std::string baselineCardName;
    std::string baselineClusterPath;
    std::string compareClusterPath;
    bool isCluster = false;
    std::shared_mutex sharedMutex;
};
}
}
}

#endif // PROFILER_SERVER_BASELINEMANAGER_H
