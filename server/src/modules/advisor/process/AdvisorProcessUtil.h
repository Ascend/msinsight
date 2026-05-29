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

#ifndef PROFILER_SERVER_ADVISORPROCESSUTIL_H
#define PROFILER_SERVER_ADVISORPROCESSUTIL_H

#include <map>
#include <vector>
#include <algorithm>
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"

namespace Dic::Module::Advisor {

class AdvisorProcessUtil {
  public:
    static void SortFlowLocationData(
        std::vector<Protocol::FlowLocation> &data, const Protocol::KernelDetailsParams &param) {
        if (param.orderBy == "name") {
            if (param.order == "ASC") {
                std::sort(data.begin(), data.end(), CompareFlowLocationByNameAsc);
            } else {
                std::sort(data.begin(), data.end(), CompareFlowLocationByNameDesc);
            }
        } else if (param.orderBy == "duration") {
            if (param.order == "ASC") {
                std::sort(data.begin(), data.end(), CompareFlowLocationByDurationAsc);
            } else {
                std::sort(data.begin(), data.end(), CompareFlowLocationByDurationDesc);
            }
        } else if (param.orderBy == "pid") {
            if (param.order == "ASC") {
                std::sort(data.begin(), data.end(), CompareFlowLocationByPidAsc);
            } else {
                std::sort(data.begin(), data.end(), CompareFlowLocationByPidDesc);
            }
        } else if (param.orderBy == "tid") {
            if (param.order == "ASC") {
                std::sort(data.begin(), data.end(), CompareFlowLocationByTidAsc);
            } else {
                std::sort(data.begin(), data.end(), CompareFlowLocationByTidDesc);
            }
        } else if (param.orderBy == "startTime") {
            if (param.order == "ASC") {
                std::sort(data.begin(), data.end(), CompareFlowLocationByTimestampAsc);
            } else {
                std::sort(data.begin(), data.end(), CompareFlowLocationByTimestampDesc);
            }
        } else {
            std::sort(data.begin(), data.end(), CompareFlowLocationByDurationDesc);
        }
    }

  private:
    static bool CompareFlowLocationByNameAsc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return a.name < b.name;
    }

    static bool CompareFlowLocationByNameDesc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return a.name > b.name;
    }

    static bool CompareFlowLocationByPidAsc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return a.pid < b.pid;
    }

    static bool CompareFlowLocationByPidDesc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return a.pid > b.pid;
    }

    static bool CompareFlowLocationByTidAsc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return a.tid < b.tid;
    }

    static bool CompareFlowLocationByTidDesc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return a.tid > b.tid;
    }

    static bool CompareFlowLocationByTimestampAsc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return a.timestamp < b.timestamp;
    }

    static bool CompareFlowLocationByTimestampDesc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return a.timestamp > b.timestamp;
    }

    static bool CompareFlowLocationByDurationAsc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return NumberSafe::Sub(a.duration, a.timestamp) < NumberSafe::Sub(b.duration, b.timestamp);
    }

    static bool CompareFlowLocationByDurationDesc(const Protocol::FlowLocation &a, const Protocol::FlowLocation &b) {
        return NumberSafe::Sub(a.duration, a.timestamp) > NumberSafe::Sub(b.duration, b.timestamp);
    }
};
} // Dic::Module::Advisor

#endif // PROFILER_SERVER_ADVISORPROCESSUTIL_H
