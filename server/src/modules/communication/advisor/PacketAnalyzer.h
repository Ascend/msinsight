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

#ifndef PROFILER_SERVER_COMMUNICATION_PACKET_ANALYZER_H
#define PROFILER_SERVER_COMMUNICATION_PACKET_ANALYZER_H

#include "CommunicationBaseAnalyzer.h"
#include "ClusterDef.h"

namespace Dic {
namespace Module {
namespace Communication {
using namespace Protocol;
using namespace Dic::Module;

struct PacketAnalyzerStatistics {
    const double smallSdmaSizeStandard{16.0};
    const double smallSdmaProportionStandard{0.2};
    uint64_t sdmaCount{0};
    uint64_t smallSdmaCount{0};
    double smallSdmaProportion{0.0};
    double smallSdmaDuration{0.0};
    bool sdmaIssue{false};

    const double smallRdmaSizeStandard{1.0};
    const double smallRdmaProportionStandard{0.2};
    uint64_t rdmaCount{0};
    uint64_t smallRdmaCount{0};
    double smallRdmaProportion{0.0};
    double smallRdmaDuration{0.0};
    bool rdmaIssue{false};
};

class PacketAnalyzer : public CommunicationBaseAnalyzer {
  public:
    PacketAnalyzer() = default;
    ~PacketAnalyzer() override = default;
    bool QueryAdvisorData(const std::string &clusterPath) override;
    void ComputeStatistics() override;
    void AssembleAdvisor(CommunicationAdvisorInfo &info) override;

  protected:
    std::vector<PacketAnalyzerData> data;
    PacketAnalyzerStatistics statistics;
};
} // end of namespace Communication
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_COMMUNICATION_PACKET_ANALYZER_H
