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

#ifndef PROFILER_SERVER_COMMUNICATION_RETRANSMISSION_H
#define PROFILER_SERVER_COMMUNICATION_RETRANSMISSION_H

#include "CommunicationBaseAnalyzer.h"
#include "ClusterDef.h"

namespace Dic {
namespace Module {
namespace Communication {
using namespace Protocol;
using namespace Dic::Module;

const double MIN_RETRANSMISSION_TIME = 4000.0;

struct RetransmissionAnalyzerStatistics {
    std::string groupName;
    std::string opName;
};

class RetransmissionAnalyzer : public CommunicationBaseAnalyzer {
  public:
    RetransmissionAnalyzer() = default;
    ~RetransmissionAnalyzer() override = default;
    bool QueryAdvisorData(const std::string &clusterPath) override;
    void ComputeStatistics() override;
    void AssembleAdvisor(CommunicationAdvisorInfo &info) override;

  protected:
    std::vector<RetransmissionClassificationInfo> data;
    std::vector<RetransmissionAnalyzerStatistics> statistics;
};
} // end of namespace Communication
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_COMMUNICATION_RETRANSMISSION_H
