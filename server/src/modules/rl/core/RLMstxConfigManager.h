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

#ifndef PROFILER_SERVER_RLMSTXCONFIGMANAGER_H
#define PROFILER_SERVER_RLMSTXCONFIGMANAGER_H
#include <vector>
#include "RLDomainObject.h"
namespace Dic::Module::RL {
class RLMstxConfigManager {
  public:
    static RLMstxConfigManager &Instance();
    RLMstxConfigManager(const RLMstxConfigManager &) = delete;
    RLMstxConfigManager &operator=(const RLMstxConfigManager &) = delete;
    RLMstxConfigManager(RLMstxConfigManager &&) = delete;
    RLMstxConfigManager &operator=(RLMstxConfigManager &&) = delete;

    std::vector<RLMstxConfig> GetRLMstxConfig();
    std::vector<std::string> GetMstxTaskNameList();
    std::string GetTaskTypeByName(const std::string &name);

    /**
     * @brief 根据taskname获取匹配的配置项
     */
    RLMstxConfig GetMstxConfigByTaskName(const std::vector<std::string> &taskNames);

  private:
    void InitConfig();
    void InitDefaultConf();
    void InitVerlGrpoConf();
    void InitMindspeedRlGrpoConf();

    explicit RLMstxConfigManager();
    ~RLMstxConfigManager() = default;

    std::vector<RLMstxConfig> config;
};
}
#endif // PROFILER_SERVER_RLMSTXCONFIGMANAGER_H
