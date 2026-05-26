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

#ifndef PROFILER_SERVER_MEMORYDEF_H
#define PROFILER_SERVER_MEMORYDEF_H

#include <string>
#include <set>
#include "FileDef.h"

namespace Dic::Module::Memory {

struct MemoryFilePairs {
    std::set<std::string> operatorFiles;
    std::set<std::string> recordFiles;
    std::set<std::string> staticOpFiles;
    std::set<std::string> componentFiles;
};

struct Record {
    std::string component;
    int64_t timesTamp;
    double totalAllocated;
    double totalReserved;
    double totalActivated;
    std::string deviceType;
    std::string streamId;
};

struct Operator {
    std::string name;
    double size;
    int64_t allocationTime;
    int64_t releaseTime;
    double duration;
    int64_t activeReleaseTime;
    double activeDuration;
    double allocationAllocated;
    double allocationReserved;
    double allocationActive;
    double releaseAllocated;
    double releaseReserved;
    double releaseActive;
    std::string streamId;
    std::string deviceType;
};

struct StaticOp {
    std::string deviceId;
    std::string opName;
    std::string modelName;
    std::string graphId;
    int64_t nodeIndexStart;
    int64_t nodeIndexEnd;
    double size;
};

struct Component {
    std::string component;
    int64_t timestamp;
    double totalReserved;
    std::string device;
};

// Type类型字段
const std::string MEMORY_TYPE_DYNAMIC = "dynamic"; // 纯动态图数据
const std::string MEMORY_TYPE_STATIC = "static"; // 纯静态图数据
const std::string MEMORY_TYPE_MIX = "mix"; // 存在动态图数据以及静态子图数据

// Resource Type类型字段
const std::string MEMORY_RESOURCE_TYPE_PYTORCH = "Pytorch";
const std::string MEMORY_RESOURCE_TYPE_MIND_SPORE = "MindSpore";

// field in operator_memory
const std::string ALLOCATION_TIME = "Allocation Time(us)";
const std::string RELEASE_TIME = "Release Time(us)";
const std::string ACTIVE_RELEASE_TIME = "Active Release Time(us)";
const std::string ACTIVE_DURATION = "Active Duration(us)";
const std::string STREAM_PTR = "Stream Ptr";
const std::string ALLOCATION_ALLOCATED_MB = "Allocation Total Allocated(MB)";
const std::string ALLOCATION_RESERVED_MB = "Allocation Total Reserved(MB)";
const std::string ALLOCATION_ACTIVE_MB = "Allocation Total Active(MB)";
const std::string RELEASE_ALLOCATED_MB = "Release Total Allocated(MB)";
const std::string RELEASE_RESERVED_MB = "Release Total Reserved(MB)";
const std::string RELEASE_ACTIVE_MB = "Release Total Active(MB)";
const std::string ALLOCATION_ALLOCATED_KB = "Allocation Total Allocated(KB)";
const std::string ALLOCATION_RESERVED_KB = "Allocation Total Reserved(KB)";
const std::string ALLOCATION_ACTIVE_KB = "Allocation Total Active(KB)";
const std::string RELEASE_ALLOCATED_KB = "Release Total Allocated(KB)";
const std::string RELEASE_RESERVED_KB = "Release Total Reserved(KB)";
const std::string RELEASE_ACTIVE_KB = "Release Total Active(KB)";
const std::vector<std::string> OPERATOR_CSV = {NAME, ALLOCATION_TIME, SIZE, DURATION};
const std::vector<std::string> OPERATOR_CSV_MSPROF = {NAME, ALLOCATION_TIME, SIZE, DURATION};

// field in memory_record
const std::string TOTAL_ALLOCATED_MB = "Total Allocated(MB)";
const std::string TOTAL_RESERVED_MB = "Total Reserved(MB)";
const std::string TOTAL_ACTIVE_MB = "Total Active(MB)";
const std::string TOTAL_ALLOCATED_KB = "Total Allocated(KB)";
const std::string TOTAL_RESERVED_KB = "Total Reserved(KB)";
const std::string TOTAL_ACTIVE_KB = "Total Active(KB)";
const std::vector<std::string> RECORD_CSV = {COMPONENT, TIMESTAMP, DEVICETYPE, TOTAL_ALLOCATED_MB, TOTAL_RESERVED_MB};
const std::vector<std::string> RECORD_CSV_MSPROF = {
    COMPONENT, TIMESTAMP, DEVICE, TOTAL_ALLOCATED_KB, TOTAL_RESERVED_KB};

// field in memory_record
const std::string OP_NAME = "Op Name";
const std::string MODEL_NAME = "Model Name";
const std::string GRAPH_ID = "Graph ID";
const std::string NODE_INDEX_START = "Node Index Start";
const std::string NODE_INDEX_END = "Node Index End";
const std::string SIZE_KB = "Size(KB)";
const std::vector<std::string> STATIC_OP_MEM_CSV = {
    DEVICE_ID, OP_NAME, MODEL_NAME, GRAPH_ID, NODE_INDEX_START, NODE_INDEX_END, SIZE_KB};

// field in npu_module_mem
const std::vector<std::string> NPU_MODULE_MEM_CSV_PYTORCH = {COMPONENT, TIMESTAMP, TOTAL_RESERVED_MB, DEVICE};
const std::vector<std::string> NPU_MODULE_MEM_CSV_MINDSPORE = {COMPONENT, TIMESTAMP, TOTAL_RESERVED_KB, DEVICE};

// table header
const std::vector<std::string> MEMORY_RECORD_HEADER = {
    COMPONENT, TIMESTAMP, TOTAL_ALLOCATED_MB, TOTAL_RESERVED_MB, TOTAL_ACTIVE_MB, DEVICETYPE, STREAM_PTR};
const std::vector<std::string> OPERATOR_MEMORY_HEADER = {NAME, SIZE, ALLOCATION_TIME, RELEASE_TIME, DURATION,
    ACTIVE_RELEASE_TIME, ACTIVE_DURATION, ALLOCATION_ALLOCATED_MB, ALLOCATION_RESERVED_MB, ALLOCATION_ACTIVE_MB,
    RELEASE_ALLOCATED_MB, RELEASE_RESERVED_MB, RELEASE_ACTIVE_MB, STREAM_PTR, DEVICETYPE};

// default page size
const uint64_t DEFAULT_PAGE_SIZE = 10;
} // end of namespace Dic::Module::Memory

#endif // PROFILER_SERVER_MEMORYDEF_H
