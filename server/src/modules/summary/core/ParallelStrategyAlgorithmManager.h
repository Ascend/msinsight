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

#ifndef PROFILER_SERVER_SERVER_SRC_MODULES_SUMMARY_CORE_PARALLELSTRATEGYALGORITHMMANAGER_H
#define PROFILER_SERVER_SERVER_SRC_MODULES_SUMMARY_CORE_PARALLELSTRATEGYALGORITHMMANAGER_H

#include <memory>
#include <mutex>
#include <map>
#include "BaseParallelStrategyAlgorithm.h"
namespace Dic::Module::Summary {

class ParallelStrategyAlgorithmManager {
  public:
    static ParallelStrategyAlgorithmManager &Instance();
    void Reset();
    bool AddOrUpdateAlgorithm(
        const std::string &projectName, const ParallelStrategyConfig &config, std::string &errMsg);
    bool DeleteAlgorithm(const std::string &projectName);
    std::shared_ptr<BaseParallelStrategyAlgorithm> GetAlgorithmByProjectName(const std::string &projectName);
    ParallelStrategyConfig GetParallelStrategyConfig(const std::string &key);

  private:
    ParallelStrategyAlgorithmManager() = default;
    ~ParallelStrategyAlgorithmManager() = default;
    static bool IsSameAlgorithm(const std::string &algorithm1, const std::string &algorithm2);
    static std::shared_ptr<BaseParallelStrategyAlgorithm> CreateAlgorithm(const std::string &algorithm);

    std::recursive_mutex mutex;
    std::map<std::string, std::shared_ptr<BaseParallelStrategyAlgorithm>> algorithmMap;
};
}
#endif // PROFILER_SERVER_SERVER_SRC_MODULES_SUMMARY_CORE_PARALLELSTRATEGYALGORITHMMANAGER_H
