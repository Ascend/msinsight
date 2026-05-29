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

#include "CommunicationAdvisor.h"
#include "PacketAnalyzer.h"
#include "ByteAlignmentAnalyzer.h"
#include "BandwidthContentionAnalyzer.h"
#include "RetransmissionAnalyzer.h"

namespace Dic {
namespace Module {
namespace Communication {
// LCOV_EXCL_BR_START
void CommunicationAdvisor::Register() {
    advisorMap.emplace(PACKET_ANALYZER_TITLE, std::make_unique<PacketAnalyzer>());
    advisorMap.emplace(BYTEALIGNMENT_ANALYZER_TITLE, std::make_unique<ByteAlignmentAnalyzer>());
    advisorMap.emplace(BANDWIDTHCONTENTION_ANALYZER_TITLE, std::make_unique<BandwidthContentionAnalyzer>());
    advisorMap.emplace(RETRANSMISSION_ANALYZER_TITLE, std::make_unique<RetransmissionAnalyzer>());
}

void CommunicationAdvisor::GenerateAdvisor(
    std::vector<CommunicationAdvisorInfo> &items, const std::string &clusterPath) {
    for (const auto &analyzer : advisorMap) {
        CommunicationAdvisorInfo info;
        if (analyzer.second->GenerateAdvisor(info, clusterPath)) {
            items.emplace_back(info);
        }
    }
}
// LCOV_EXCL_BR_STOP
}
}
}
