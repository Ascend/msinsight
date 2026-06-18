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

#ifndef DICTOOL_PYTHON_STACK_HELPER_H
#define DICTOOL_PYTHON_STACK_HELPER_H

#include <set>
#include <string>

#include "FullDbEnumUtil.h"
#include "TimelineProtocolRequest.h"

namespace Dic {
namespace Module {
namespace Timeline {
class PythonStackHelper {
  public:
    static bool TryParseThreadId(const std::string &threadId, std::string &realThreadId) {
        if (threadId.rfind(GetTextPythonStackThreadIdPrefix(), 0) == 0) {
            realThreadId = threadId.substr(GetTextPythonStackThreadIdPrefix().length());
            return !realThreadId.empty();
        }
        const auto &prefix = Protocol::PYTHON_STACK_THREAD_ID_PREFIX;
        if (threadId.rfind(prefix, 0) != 0) {
            return false;
        }
        realThreadId = threadId.substr(prefix.length());
        return !realThreadId.empty();
    }

    static bool RestoreThreadIdAndMetaType(const std::string &processId, std::string &threadId, std::string &metaType) {
        std::string realThreadId;
        if (threadId.rfind(GetTextPythonStackThreadIdPrefix(), 0) == 0) {
            realThreadId = threadId.substr(GetTextPythonStackThreadIdPrefix().length());
            if (realThreadId.empty()) {
                return false;
            }
            threadId = realThreadId;
            metaType = ENUM_TO_STR(PROCESS_TYPE::TEXT).value_or("");
            return true;
        }
        if (TryParseThreadId(threadId, realThreadId)) {
            (void)processId;
            threadId = Protocol::PYTHON_API_THREAD_ID;
            metaType = ENUM_TO_STR(PROCESS_TYPE::API).value_or("");
            return true;
        }
        return false;
    }

    static bool RestoreMetadata(Protocol::Metadata &metadata) {
        bool restored = RestoreThreadIdAndMetaType(metadata.pid, metadata.tid, metadata.metaType);
        if (restored) {
            metadata.isPythonStack = true;
        }
        return restored;
    }

    static bool RestoreThreadTracesParams(Protocol::UnitThreadTracesParams &params) {
        bool restored = RestoreThreadIdAndMetaType(params.processId, params.threadId, params.metaType);
        if (restored) {
            params.isPythonStack = true;
        }
        return restored;
    }

    static bool RestoreThreadDetailParams(Protocol::ThreadDetailParams &params) {
        bool restored = RestoreThreadIdAndMetaType(params.pid, params.tid, params.metaType);
        if (restored) {
            params.isPythonStack = true;
        }
        return restored;
    }

    static bool RestoreUnitFlowsParams(Protocol::UnitFlowsParams &params) {
        return RestoreThreadIdAndMetaType(params.pid, params.tid, params.metaType);
    }

    static void RestoreEventsViewParams(Protocol::EventsViewParams &params) {
        bool restored = RestoreThreadIdAndMetaType(params.pid, params.tid, params.metaType);
        std::string restoredMetaType = params.metaType;
        for (auto &threadId : params.threadIdList) {
            std::string metaType = restoredMetaType;
            if (RestoreThreadIdAndMetaType(params.pid, threadId, metaType)) {
                restoredMetaType = metaType;
                restored = true;
            }
        }
        if (restored) {
            params.metaType = restoredMetaType;
            params.isPythonStack = true;
        }
    }

    static std::string GetPythonStackDisplayMetaType() { return GetPythonStackMetaType(); }

    static const std::string &GetPythonStackMetaType() {
        static const std::string metaType = ENUM_TO_STR(PROCESS_TYPE::PYTHON_STACK).value_or("");
        return metaType;
    }

    static void RestoreSameOperatorParams(Protocol::UnitThreadsOperatorsParams &params) {
        std::string realPythonStackMetaType = GetRealPythonStackMetaType(params);
        for (auto &metaType : params.metaTypeList) {
            if (metaType == GetPythonStackMetaType()) {
                metaType = realPythonStackMetaType;
                params.isPythonStack = true;
            }
        }
        for (auto &process : params.processes) {
            std::set<std::string> restoredTidList;
            for (auto tid : process.tidList) {
                std::string metaType = realPythonStackMetaType;
                if (RestoreThreadIdAndMetaType(process.pid, tid, metaType)) {
                    params.isPythonStack = true;
                }
                restoredTidList.emplace(tid);
            }
            process.tidList = restoredTidList;
        }
    }

  private:
    static const std::string &GetTextPythonStackThreadIdPrefix() {
        static const std::string prefix = Protocol::PYTHON_STACK_THREAD_ID_PREFIX + "text:";
        return prefix;
    }

    static std::string GetRealPythonStackMetaType(const Protocol::UnitThreadsOperatorsParams &params) {
        for (const auto &process : params.processes) {
            for (auto threadId : process.tidList) {
                std::string metaType = GetPythonStackMetaType();
                RestoreThreadIdAndMetaType(process.pid, threadId, metaType);
                if (metaType == ENUM_TO_STR(PROCESS_TYPE::API).value_or("")) {
                    return metaType;
                }
            }
        }
        return ENUM_TO_STR(PROCESS_TYPE::TEXT).value_or("");
    }
};
} // Timeline
} // Module
} // Dic

#endif // DICTOOL_PYTHON_STACK_HELPER_H
