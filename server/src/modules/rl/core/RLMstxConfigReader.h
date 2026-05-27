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
#ifndef PROFILER_SERVER_RLMSTXCONFIGREADER_H
#define PROFILER_SERVER_RLMSTXCONFIGREADER_H

#include <string>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include "JsonUtil.h"
#include "RLDomainObject.h"

namespace Dic::Module::RL {
class RLMstxConfigReader {
  public:
    explicit RLMstxConfigReader();

    std::vector<RLMstxConfig> ReadConfigFile();

    void SetConfigPath(const std::string &path);

  private:
    bool ConfigFormatValid(document_t &doc);

    static bool MstxConfigCheck(const json_t &mstxConfigJson);

    static bool TaskConfigCheck(const json_t &taskConfJson);

    static bool MicroBatchConfigCheck(const json_t &microBatchJson);

    RLMstxConfig ParseMstxConfig(const json_t &config);

    TaskConfig ParseTaskConfig(const json_t &taskConfJson);

    MicroBatchConfig ParseMicroBatchConfig(const json_t &microBatchConfJson);

    std::string configPath;
};

} // Dic
// Module
// RL

#endif // PROFILER_SERVER_RLMSTXCONFIGREADER_H
