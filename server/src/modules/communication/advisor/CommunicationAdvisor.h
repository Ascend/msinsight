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

#ifndef PROFILER_SERVER_COMMUNICATION_ADVISOR_H
#define PROFILER_SERVER_COMMUNICATION_ADVISOR_H

#include "CommunicationBaseAnalyzer.h"

namespace Dic {
namespace Module {
namespace Communication {
using namespace Protocol;

class CommunicationAdvisor {
  public:
    CommunicationAdvisor() = default;
    ~CommunicationAdvisor() = default;
    void GenerateAdvisor(std::vector<CommunicationAdvisorInfo> &items, const std::string &clusterPath);
    void Register();

  protected:
    std::map<std::string, std::unique_ptr<CommunicationBaseAnalyzer>> advisorMap;
};
} // end of namespace Communication
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_COMMUNICATION_ADVISOR_H
