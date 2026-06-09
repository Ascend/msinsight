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

#ifndef PROFILER_SERVER_TRACE_DATABASE_DEF_H
#define PROFILER_SERVER_TRACE_DATABASE_DEF_H

#include <string>
#include <vector>

namespace Dic {
namespace Module {
namespace Timeline {
struct SliceDto {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    uint64_t duration = 0;
    uint32_t depth = 0;
    uint64_t trackId = 0;
    std::string name;
    std::string args;
    std::string cat;
    std::string flagId;
    std::string groupId;
    std::string pid;
    std::string tid;
    std::string cardId;
    std::string metaType;
};

struct FlowDetailDto {
    std::string id;
    std::string name;
    std::string cat;
    std::string flowId;
    std::string pid;
    std::string tid;
    uint32_t depth = 0;
    uint64_t timestamp = 0;
    uint64_t flowTimestamp = 0;
    std::string type;
    uint64_t trackId = 0;
};
// flow连接着两个界面元素，s标识flow的起点元素块，f标识flow的终点元素块，它们分别都有起始时间和结束时间
// 下面是flow的起点元素块
// -----|***********************|--------------------
// -----^sStartTime-------------^sEndTime------------
// 下面是flow的终点元素块
//--------------------|***************|--------------
//--------------------^fStartTime-----^fEndTime------
struct FlowStartAndEndTime {
    uint64_t sStartTime = 0;
    uint64_t sEndTime = 0;
    uint64_t fStartTime = 0;
    uint64_t fEndTime = 0;
};

struct MetaDataDto {
    std::string pid;
    std::string processName;
    std::string metaType;
    std::string label;
    std::string threadId;
    std::string threadName;
    std::string groupNameValue;
    int32_t maxDepth = 0;
    std::string name; // ph = C, name
    std::string args; // ph = C, args
};

struct OneKernelData {
    std::string threadId;
    std::string pid;
};

struct LayerStatData {
    uint64_t total = 0;
    double allOperatorTime = 0.1;
};

struct KernelShapesDataDto {
    std::string inputShapes;
    std::string inputDataTypes;
    std::string inputFormats;
    std::string outputShapes;
    std::string outputDataTypes;
    std::string outputFormats;
};

struct AICpuCheckDataType {
    std::vector<std::string> input;
    std::vector<std::string> output;
    std::string note;
};

struct FuseableOpRule {
    std::vector<std::string> opList;
    std::string fusedOp;
    std::string note;
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
#endif // PROFILER_SERVER_TRACE_DATABASE_DEF_H
