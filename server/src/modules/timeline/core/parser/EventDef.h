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

#ifndef PROFILER_SERVER_EVENT_DEF_H
#define PROFILER_SERVER_EVENT_DEF_H

#include <string>
#include <optional>
#include <map>

namespace Dic {
namespace Module {
namespace Timeline {
namespace Trace {
struct Event {
    virtual ~Event() = default;
    std::string type;
};

struct Slice : public Event {
    uint64_t trackId = 0;
    std::string tid;
    int64_t ts = 0;
    int64_t dur = 0;
    int64_t end = 0;
    std::string pid;
    std::string name;
    std::string processName;
    std::string threadName;
    std::string cname;
    std::optional<std::string> cat;
    std::optional<std::string> args;
    std::string flagId;
    std::string groupId;
};

struct MetaDataArgs {
    std::string name;
    std::string labels;
    int64_t sortIndex = 0;
};

struct MetaData : public Event {
    uint64_t trackId = 0;
    std::string tid;
    std::string name;
    std::string pid;
    MetaDataArgs args;
};

struct ThreadEvent : public Event {
    uint64_t trackId = 0;
    std::string tid;
    std::string pid;
    std::string threadName;
    uint32_t threadSortIndex = 0;
    bool operator < (const ThreadEvent &right) const
    {
        if (trackId < right.trackId) {
            return true;
        }
        return false;
    }

    void SetThreadSortIndex()
    {
        static std::map<std::string, uint32_t> orderMap = { { "SCALAR", 1 }, { "FLOWCTRL", 2 }, { "MTE1", 3 },
                                                            { "CUBE", 4 },   { "FIXP", 5 },     { "MTE2", 6 },
                                                            { "VECTOR", 7 }, { "MTE3", 8 },     { "CACHEMISS", 9 } };
        if (orderMap.find(threadName) != orderMap.end()) {
            threadSortIndex = orderMap[threadName];
        } else {
            threadSortIndex = 0;
        }
    }
};

struct ProcessEvent : public Event {
    std::string pid;
    std::string processName;
    bool operator < (const ProcessEvent &right) const
    {
        if (pid < right.pid) {
            return true;
        }
        return false;
    }
};

struct Flow : public Event {
    uint64_t trackId = 0;
    std::string tid;
    int64_t ts = 0;
    std::string pid;
    std::string flowId;
    std::string name;
    std::optional<std::string> cat;
};

struct Counter : public Event {
    std::string name;
    std::string pid;
    int64_t ts = 0;
    std::optional<std::string> cat;
    std::string args;
    std::string tid;
    uint64_t trackId = 0;
};
} // end of namespace Trace
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
#endif // PROFILER_SERVER_EVENT_DEF_H