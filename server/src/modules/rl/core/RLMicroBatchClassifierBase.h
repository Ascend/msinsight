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

#ifndef PROFILER_SERVER_RLMICROBATCHCLASSIFIERBASE_H
#define PROFILER_SERVER_RLMICROBATCHCLASSIFIERBASE_H
#include <vector>
#include "RLProtocolResponse.h"
#include "RLDomainObject.h"

namespace Dic::Module::RL {
enum State : int { Init = 0, FP = 1, BP = 2, Communication = 3 };
/**
 * @brief 算法基类，用于支持不同后端的不同microBatch划分逻辑
 */
class RLMicroBatchClassifierBase {
  public:
    /**
     * @brief 入口函数
     */
    std::vector<Protocol::RLPipelineNode> GetClassifiedMicroBatch(
        const std::string &fileId, const RLMstxConfig &config, const Protocol::RLPipelineNode &taskNode) {
        auto original = QueryMicroBatchSlices(fileId, config, taskNode);
        return MicroBatchClassifier(original);
    }

  protected:
    virtual std::vector<Protocol::RLPipelineNode> QueryMicroBatchSlices(
        const std::string &fileId, const RLMstxConfig &config, const Protocol::RLPipelineNode &taskNode) = 0;

    virtual std::vector<Protocol::RLPipelineNode> MicroBatchClassifier(
        std::vector<Protocol::RLPipelineNode> &nodes) = 0;
};
}
#endif // PROFILER_SERVER_RLMICROBATCHCLASSIFIERBASE_H
