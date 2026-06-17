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

#ifndef DIC_SOURCE_PROTOCOL_SOURCE_RESPONSE_H
#define DIC_SOURCE_PROTOCOL_SOURCE_RESPONSE_H

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cmath>
#include "GlobalDefs.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
#include "SourceDefs.h"

namespace Dic::Protocol {
struct SourceCodeFileResBody {
    std::string fileContent;
};

struct SourceCodeFileResponse : public Response {
    SourceCodeFileResponse() : Response(REQ_RES_SOURCE_CODE_FILE) {}

    SourceCodeFileResBody body;
};

struct SourceFileLineRes {
    int line{};
    float cycle{};
    int instructionExecuted{};
    std::vector<std::pair<std::string, std::string>> addressRange;
};

struct SourceApiLineResBody {
    std::vector<SourceFileLineRes> lines;
};

struct SourceApiLineResponse : public Response {
    SourceApiLineResponse() : Response(REQ_RES_SOURCE_API_LINE) {}

    SourceApiLineResBody body;
};

struct SourceApiInstrResBody {
    std::string instructions;
};

struct SourceApiInstrResponse : public Response {
    SourceApiInstrResponse() : Response(REQ_RES_SOURCE_API_INSTRUCTIONS) {}

    SourceApiInstrResBody body;
};

struct SourceColumnValueMap {
    std::unordered_map<std::string, int> intMap;
    std::unordered_map<std::string, float> floatMap;
    std::unordered_map<std::string, std::string> stringMap;
    std::unordered_map<std::string, Dic::Module::Source::PercentageAndDetails> percentAndDetailsColumnMap;
};

struct SourceApiInstrRes {
    std::string address;
    std::string ascendCInnerCode;
    int cycles{};
    int instructionsExecuted{};
    std::string pipe;
    std::string source;
    int theoreticalStallCycles{};
    int realStallCycles{};
};

struct SourceApiInstrDynamicBody {
    std::string coreName;
    std::map<std::string, int> columnNameMap;
    std::vector<SourceColumnValueMap> columnValues;
    // earlier version data
    std::vector<SourceApiInstrRes> instructions;
};

struct SourceApiInstrDynamicResponse : public Response {
    SourceApiInstrDynamicResponse() : Response(REQ_RES_SOURCE_API_INSTRUCTIONS_DYNAMIC) {}
    SourceApiInstrDynamicBody body;
};

struct SourceFileLineDynamic {
    SourceColumnValueMap columnValueMap;
    std::vector<std::pair<std::string, std::string>> addressRange;
};

struct SourceApiLineDynamicResBody {
    std::map<std::string, int> columnNameMap;
    std::vector<SourceFileLineDynamic> sourceFileLines;
    // earlier version data
    std::vector<SourceFileLineRes> lines;
};

struct SourceApiLineDynamicResponse : public Response {
    SourceApiLineDynamicResponse() : Response(REQ_RES_SOURCE_API_LINE_DYNAMIC) {}

    SourceApiLineDynamicResBody body;
};

struct TableRow {
    std::string name;
    std::vector<std::string> value;

    void BaseInfoClone(const TableRow &row)
    {
        name = row.name;
        value.clear();
        for (size_t i = 0; i < row.value.size(); ++i) {
            value.emplace_back("-");
        }
    }
};

template<typename R>
struct TableDetail {
    std::string tableName;
    std::vector<std::string> size;
    std::vector<std::string> headerName;
    std::vector<R> row;
};

struct DetailsBaseInfoResBody {
    std::string name;
    std::string soc;
    std::string opType;
    std::string blockDim;
    std::string mixBlockDim;
    std::string duration;
    std::string deviceId;
    std::string pid;
    TableDetail<TableRow> blockDetail;
    std::vector<std::string> advice;
};

struct DetailsBaseInfoResponse : public Response {
    DetailsBaseInfoResponse() : Response(REQ_RES_DETAILS_BASE_INFO) {}
    CompareData<DetailsBaseInfoResBody> body;
};

struct SubBlockUnitData {
    std::string blockId;
    std::string blockType;
    std::string name;
    std::string unit;
    std::string value = "-";
    std::string originValue = "-";
    void BaseInfoClone(const SubBlockUnitData &origin)
    {
        blockId = origin.blockId;
        blockType = origin.blockType;
        name = origin.name;
        unit = origin.unit;
    }
};

struct Roofline {
    std::string bw;   // 理论带宽
    std::string bwName;
    std::string computility;  // 屋顶算力
    std::string computilityName;    // 算力名称
    std::vector<std::string> point;  // 坐标
    std::string ratio;      // 百分比
};

struct RooflineGraph {
    // 标题
    std::string title;
    std::vector<Roofline> rooflines;
};

struct RooflineData {
    std::string advice;
    std::vector<RooflineGraph> multipleRooflines;
};

struct SubBlockData {
    std::vector<CompareData<SubBlockUnitData>> detailDataList;
    std::vector<std::string> advice;
};

struct DetailsLoadInfoResBody {
    std::vector<std::string> blockIdList;
    SubBlockData chartData;
    SubBlockData tableData;
};

struct DetailsLoadInfoResponse : public Response {
    DetailsLoadInfoResponse() : Response(REQ_RES_DETAILS_COMPUTE_LOAD_INFO) {}
    DetailsLoadInfoResBody body;
};

struct MemoryUnit {
    std::string memoryPath;
    std::string request;
    std::string requestSuffix;
    std::string bandwidth;
    std::string bandwidthSuffix;
    std::string peakRatio;
    bool display;
};

struct L2Cache {
    std::string hit;
    std::string miss;
    std::string totalRequest;
    std::string hitRatio;
};

struct UtilizationRate {
    std::string cycle;
    std::string totalCycles;
    std::string ratio;
};

struct MemoryGraph {
    std::string blockId;
    std::string blockType;
    std::string chipType;
    std::vector<CompareData<MemoryUnit>> memoryUnit;
    CompareData<L2Cache> l2Cache;
    std::vector<std::string> advice;
    CompareData<UtilizationRate> vector;
    CompareData<UtilizationRate> vector1;
    CompareData<UtilizationRate> cube;
};

struct MemoryTable {
    std::string blockId;
    std::string tableOpType;
    std::vector<TableDetail<CompareData<TableRow>>> tableDetail;
    std::vector<std::string> advice;

    void FillBaseInfoFromCompare()
    {
        for (auto &item : tableDetail) {
            for (auto &row : item.row) {
                TableRow &baseline = row.baseline;
                baseline.BaseInfoClone(row.compare);
                TableRow &diff = row.diff;
                diff.BaseInfoClone(row.compare);
            }
        }
    }
};
struct DetailsMemoryGraphResBody {
    std::vector<MemoryGraph> coreMemory;
};

struct DetailsMemoryGraphResponse : public Response {
    DetailsMemoryGraphResponse() : Response(REQ_RES_DETAILS_COMPUTE_MEMORY_GRAPH) {};
    DetailsMemoryGraphResBody body;
};

struct DetailsMemoryTableResBody {
    std::vector<MemoryTable> memoryTable;
};

struct DetailsMemoryTableResponse : public Response {
    DetailsMemoryTableResponse() : Response(REQ_RES_DETAILS_COMPUTE_MEMORY_TABLE) {};
    DetailsMemoryTableResBody body;
};

template<typename T>
struct DetailsInterCoreLoadDimension {
    CompareData<T> value;
    int level = 0;
};

struct DetailsInterCoreLoadSubCoreDetail {
    static const uint8_t maxLevel = 10;
    std::string subCoreName;
    DetailsInterCoreLoadDimension<uint64_t> cycles = {{0, 0, 0}};
    DetailsInterCoreLoadDimension<uint64_t> throughput = {{0, 0, 0}};
    DetailsInterCoreLoadDimension<float> cacheHitRate = {{0, 0, 0}};
    DetailsInterCoreLoadDimension<uint64_t> simtVfInstructions = {{0, 0, 0}};
    DetailsInterCoreLoadDimension<float> simtVfInstructionPerCycle = {{0, 0, 0}};

    void SetCyclesDimension(const int64_t curCycles, const long double average, long double sigma)
    {
        cycles.value.compare = curCycles;
        if (std::abs(sigma) < std::numeric_limits<long double>::epsilon()) {
            cycles.level = 0;
            return;
        }
        const long double core = (static_cast<long double>(curCycles) - average) / sigma;
        const long double sigmod = static_cast<long double>(1) / (static_cast<long double>(1) + std::exp(-core));
        const int result = static_cast<int>(sigmod * 10);
        cycles.level = result;
    }

    void SetThroughputDimension(int64_t curThroughput, long double average, long double sigma)
    {
        throughput.value.compare = curThroughput;
        if (std::abs(sigma) < std::numeric_limits<long double>::epsilon()) {
            throughput.level = 0;
            return;
        }
        const long double core = (static_cast<long double>(curThroughput) - average) / sigma;
        const long double sigmod = static_cast<long double>(1) / (static_cast<long double>(1) + std::exp(-core));
        const int result = static_cast<int>(sigmod * 10);
        throughput.level = result;
    }

    void SetCacheHitRateDimension(float curRate, long double averageRate, long double sigma)
    {
        cacheHitRate.value.compare = curRate;
        if (std::abs(sigma) < std::numeric_limits<long double>::epsilon()) {
            cacheHitRate.level = 0;
            return;
        }
        const long double core = (static_cast<long double>(curRate) - averageRate) / sigma;
        const long double sigmod = static_cast<long double>(1) / (static_cast<long double>(1) + std::exp(-core));
        const int result = static_cast<int>(sigmod * 10);
        cacheHitRate.level = result;
    }

    void SetSubCoreName(const std::string& type, uint8_t id)
    {
        subCoreName = type + std::to_string(id);
    }

    void SetSimtVfInstruction(uint64_t curSimtVfInstructions, long double averageSimtVfInstructions, long double sigmaSimtVfInstructions)
    {
        simtVfInstructions.value.compare = curSimtVfInstructions;
        if (std::abs(sigmaSimtVfInstructions) < std::numeric_limits<long double>::epsilon()) {
            simtVfInstructions.level = 0;
            return;
        }
        const long double core = (static_cast<long double>(curSimtVfInstructions) - averageSimtVfInstructions) / sigmaSimtVfInstructions;
        const long double sigmod = static_cast<long double>(1) / (static_cast<long double>(1) + std::exp(-core));
        const int result = static_cast<int>(sigmod * 10);
        simtVfInstructions.level = result;
    }
    void SetSimtVfInstructionPerCycle(float curSimtVfInstructionPerCycle)
    {
        simtVfInstructionPerCycle.value.compare = curSimtVfInstructionPerCycle;
    }
};

struct DetailsInterCoreLoadOpDetail {
    uint8_t coreId = 0;
    std::vector<DetailsInterCoreLoadSubCoreDetail> subCoreDetails = {};

    void AddSubCoreDetail(DetailsInterCoreLoadSubCoreDetail&& subCoreDetail)
    {
        subCoreDetails.emplace_back(std::move(subCoreDetail));
    }
};

struct DetailsInterCoreLoadGraphBody {
    std::string soc;
    std::string opType;
    std::string advice;
    std::vector<DetailsInterCoreLoadOpDetail> opDetails = {};

    void AddOpDetail(DetailsInterCoreLoadOpDetail&& opDetail)
    {
        opDetails.emplace_back(std::move(opDetail));
    }
};

struct DetailsInterCoreLoadGraphResponse : public Response {
    DetailsInterCoreLoadGraphResponse() : Response(REQ_RES_DETAILS_INTER_CORE_LOAD_GRAPH) {};
    DetailsInterCoreLoadGraphBody body;
};

struct DetailsRooflineBody {
    std::string soc;
    std::string advice;
    std::vector<RooflineGraph> data;
};

struct DetailsRooflineResponse : public Response {
    DetailsRooflineResponse() : Response(std::string(REQ_RES_DETAILS_ROOFLINE)) {};
    DetailsRooflineBody body;
};

struct CacheLineRecordResBody {
    std::string cachelineRecords;
};
 
struct CachelineRecordResponse : public Response {
    CachelineRecordResponse() : Response(std::string(REQ_RES_CACHELINE_RECORD)) {}
    CacheLineRecordResBody body;
};

struct StallReasonItem {
    std::string name;
    int64_t value = 0;
};

struct TopWarpStallReasonResBody {
    std::string unit;
    std::vector<StallReasonItem> data;
};

struct TopWarpStallReasonResponse : public Response {
    TopWarpStallReasonResponse() : Response(std::string(REQ_RES_TOP_WARP_STALL_REASON)) {}
    TopWarpStallReasonResBody body;
};
}

#endif // DIC_SOURCE_PROTOCOL_SOURCE_RESPONSE_H
