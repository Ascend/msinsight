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

#ifndef PROFILER_SERVER_RLPIPELINESERVICE_H
#define PROFILER_SERVER_RLPIPELINESERVICE_H

#include <unordered_map>
#include <vector>
#include <set>
#include "pch.h"
#include "RLProtocolResponse.h"
#include "VirtualTraceDatabase.h"
#include "DomainObject.h"
#include "RLDomainObject.h"
#include "RLMicroBatchMegatronClassifier.h"

namespace Dic::Module::RL {
class RLPipelineService {
  public:
    static RLPipelineService &Instance();
    bool GetPipelineInfo(Protocol::RLPipelineResponse &response);

  private:
    void Clear();
    std::vector<Protocol::RLPipelineNode> SearchNode(const std::string &rankId);
    std::vector<Protocol::RLPipelineNode> QueryMicroBatchByTask(const std::string &fileId,
        const std::vector<Protocol::RLPipelineNode> &taskList, const RLMstxConfig &taskConfig);
    void QueryPipelineByRankId(const std::string &rankIdWithHost);
    void FillPipelineMap(const std::string &originHostName, const std::string &rankId,
        const std::vector<Protocol::RLPipelineNode> &pipeline,
        std::unordered_map<std::string, RLPipelineItem> &targetMap);
    void FillAndProcessPipelineData(
        std::unordered_map<std::string, RLPipelineItem> &pipelineMap, std::vector<RLPipelineItem> &pipelineData);
    std::vector<Protocol::RLPipelineNode> QueryMicroBatch(
        const std::string &fileId, const RLMstxConfig &config, const Protocol::RLPipelineNode &node);

    RLBackEndType GetBackendType(const std::string &rankId);
    std::mutex mtx;
    uint64_t minTime;
    uint64_t maxTime;
    std::set<std::string> stageTypeList;
    std::unordered_map<std::string, RLPipelineItem> taskPipelineMap;
    std::unordered_map<std::string, RLPipelineItem> microBatchPipelineMap;
    RLBackEndType rlBackEndType{RLBackEndType::Unknown};
    std::string framework;
};
}
#endif // PROFILER_SERVER_RLPIPELINESERVICE_H
