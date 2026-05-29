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

#ifndef PROFILER_SERVER_COMMUNICATION_BASE_ANALYZER_H
#define PROFILER_SERVER_COMMUNICATION_BASE_ANALYZER_H

#include "CommunicationProtocolResponse.h"

namespace Dic {
namespace Module {
namespace Communication {
using namespace Dic::Protocol;

class CommunicationBaseAnalyzer {
  public:
    CommunicationBaseAnalyzer() = default;
    virtual ~CommunicationBaseAnalyzer() = default;
    bool GenerateAdvisor(CommunicationAdvisorInfo &info, const std::string &clusterPath);
    virtual bool QueryAdvisorData(const std::string &clusterPath) = 0;
    virtual void ComputeStatistics() = 0;
    virtual void AssembleAdvisor(CommunicationAdvisorInfo &info) = 0;
};
}
}
}

#endif // PROFILER_SERVER_COMMUNICATION_BASE_ANALYZER_H
