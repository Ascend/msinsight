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

#ifndef PROFILER_SERVER_COMMUNICATION_BYTEALIGNMENT_ANALYZER_H
#define PROFILER_SERVER_COMMUNICATION_BYTEALIGNMENT_ANALYZER_H

#include "CommunicationBaseAnalyzer.h"
#include "ClusterDef.h"

namespace Dic {
namespace Module {
namespace Communication {
using namespace Protocol;
using namespace Dic::Module;

const unsigned int BYTE_ALIGNMENT_SMALL_SIZE = 512;
const unsigned int BYTE_ALIGNMENT_BASE_SIZE = 512;

struct ByteAlignmentAnalyzerStatistics {
    std::string rankId;
    std::string name;
};

class ByteAlignmentAnalyzer : public CommunicationBaseAnalyzer {
  public:
    ByteAlignmentAnalyzer() = default;
    ~ByteAlignmentAnalyzer() override = default;
    bool QueryAdvisorData(const std::string &clusterPath) override;
    void ComputeStatistics() override;
    void AssembleAdvisor(CommunicationAdvisorInfo &info) override;

  protected:
    bool Check(const CommunicationLargeOperatorInfo &item);
    std::map<std::string, std::vector<CommunicationLargeOperatorInfo>> data;
    std::vector<ByteAlignmentAnalyzerStatistics> statistics;
};
} // end of namespace Communication
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_COMMUNICATION_BYTEALIGNMENT_ANALYZER_H
