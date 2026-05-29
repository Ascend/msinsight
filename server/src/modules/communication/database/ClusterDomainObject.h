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

#ifndef PROFILER_SERVER_CLUSTERDOMAINOBJECT_H
#define PROFILER_SERVER_CLUSTERDOMAINOBJECT_H

#include <string>
#include <vector>
#include "CommunicationProtocolResponse.h"

namespace Dic {
namespace Module {
struct MatrixInfoDo {
    int srcRank;
    int dstRank;
    std::string transportType;
    std::string opName;
    double transitSize = 0;
    double transitTime = 0;
    double bandwidth = 0;
};

struct OperatorTimeDo {
    std::string rankId;
    std::string operatorName;
    uint64_t startTime = 0;
    uint64_t elapseTime = 0;
};

struct DurationDo {
    std::string rankId;
    double startTime;
    double elapseTime = 0;
    double transitTime = 0;
    double synchronizationTime = 0;
    double waitTime = 0;
    double idleTime = 0;
    double synchronizationTimeRatio = 0;
    double waitTimeRatio = 0;
    double sdmaBw = 0;
    double rdmaBw = 0;
    double sdmaTime = 0;
    double rdmaTime = 0;
};

struct GroupInfoDo {
    std::string rankSet;
    std::string groupIdHash;
    std::string pgName;
};

struct OpTypeStatistics {
    uint64_t count;
    std::string opType;
    std::string groupIdHash;
};
}
}
#endif // PROFILER_SERVER_CLUSTERDOMAINOBJECT_H
