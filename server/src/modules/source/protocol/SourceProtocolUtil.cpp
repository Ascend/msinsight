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

#include <unordered_set>
#include "pch.h"
#include "SourceProtocol.h"
#include "SourceProtocolUtil.h"

namespace Dic {
namespace Protocol {
using namespace Dic::Server;
using namespace rapidjson;
#pragma region <<Response to json>>

template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response) {
    ServerLog::Warn("Function to response json is not implemented. command:", response.command);
    return std::nullopt;
}

template <> std::optional<document_t> ToResponseJson<SourceCodeFileResponse>(const SourceCodeFileResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "fileContent", response.body.fileContent, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<SourceApiLineResponse>(const SourceApiLineResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);

    json_t lines(kArrayType);
    for (auto lineRes : response.body.lines) {
        json_t line(kObjectType);
        JsonUtil::AddMember(line, "Line", lineRes.line, allocator);
        JsonUtil::AddMember(line, "Instruction Executed", lineRes.instructionExecuted, allocator);
        JsonUtil::AddMember(line, "Cycle", lineRes.cycle, allocator);
        json_t ranges(kArrayType);
        for (auto pair : lineRes.addressRange) {
            json_t range(kArrayType);
            range.PushBack(json_t().SetString(pair.first.c_str(), allocator), allocator);
            range.PushBack(json_t().SetString(pair.second.c_str(), allocator), allocator);
            ranges.PushBack(range, allocator);
        }
        JsonUtil::AddMember(line, "Address Range", ranges, allocator);
        lines.PushBack(line, allocator);
    }
    JsonUtil::AddMember(body, "lines", lines, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<SourceApiLineDynamicResponse>(const SourceApiLineDynamicResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    if (response.body.columnNameMap.empty()) { // 如果列名映射表为空，说明是老版本数据
        SetSourceApiLineResponseBody(response.body.lines, body, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
    // 组装表头信息
    json_t jsonFileLineDtype(kObjectType);
    json_t jsonInstrColumn(kObjectType);
    for (const auto &item : response.body.columnNameMap) {
        JsonUtil::AddMember(jsonInstrColumn, item.first, item.second, allocator);
    }
    JsonUtil::AddMember(jsonFileLineDtype, "Lines", jsonInstrColumn, allocator);

    json_t lines(kArrayType);
    for (const auto &sourceFileLine : response.body.sourceFileLines) {
        json_t jsonLine(kObjectType);
        // 组装单值数据
        for (const auto &stringItem : sourceFileLine.columnValueMap.stringMap) {
            JsonUtil::AddMember(jsonLine, stringItem.first, stringItem.second, allocator);
        }
        for (const auto &intItem : sourceFileLine.columnValueMap.intMap) {
            JsonUtil::AddMember(jsonLine, intItem.first, intItem.second, allocator);
        }
        for (const auto &floatItem : sourceFileLine.columnValueMap.floatMap) {
            JsonUtil::AddMember(jsonLine, floatItem.first, floatItem.second, allocator);
        }
        // 组装指令地址范围数据
        json_t ranges(kArrayType);
        for (auto pair : sourceFileLine.addressRange) {
            json_t range(kArrayType);
            range.PushBack(json_t().SetString(pair.first.c_str(), allocator), allocator);
            range.PushBack(json_t().SetString(pair.second.c_str(), allocator), allocator);
            ranges.PushBack(range, allocator);
        }
        JsonUtil::AddMember(jsonLine, "Address Range", ranges, allocator);
        for (const auto &percentageAndDetailsItem : sourceFileLine.columnValueMap.percentAndDetailsColumnMap) {
            std::optional<document_t> jsonItem = PercentageAndDetailsToJson(percentageAndDetailsItem.second, allocator);
            JsonUtil::AddMember(jsonLine, percentageAndDetailsItem.first, jsonItem.value(), allocator);
        }
        lines.PushBack(jsonLine, allocator);
    }
    JsonUtil::AddMember(body, "Lines", lines, allocator);
    JsonUtil::AddMember(body, "Files Dtype", jsonFileLineDtype, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

void SetSourceApiLineResponseBody(
    const std::vector<SourceFileLineRes> &lines, json_t &body, Document::AllocatorType &allocator) {
    json_t jsonLines(kArrayType);
    for (auto lineRes : lines) {
        json_t line(kObjectType);
        JsonUtil::AddMember(line, "Line", lineRes.line, allocator);
        JsonUtil::AddMember(line, "Instructions Executed", lineRes.instructionExecuted, allocator);
        JsonUtil::AddMember(line, "Cycles", lineRes.cycle, allocator);
        json_t ranges(kArrayType);
        for (auto pair : lineRes.addressRange) {
            json_t range(kArrayType);
            range.PushBack(json_t().SetString(pair.first.c_str(), allocator), allocator);
            range.PushBack(json_t().SetString(pair.second.c_str(), allocator), allocator);
            ranges.PushBack(range, allocator);
        }
        JsonUtil::AddMember(line, "Address Range", ranges, allocator);
        jsonLines.PushBack(line, allocator);
    }
    JsonUtil::AddMember(body, "Lines", jsonLines, allocator);
}

template <> std::optional<document_t> ToResponseJson<SourceApiInstrResponse>(const SourceApiInstrResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "instructions", response.body.instructions, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

void SetApiInstructionResponseBody(
    const std::vector<SourceApiInstrRes> &instructions, json_t &body, Document::AllocatorType &allocator) {
    json_t jsonInstructions(kArrayType);
    for (const auto &item : instructions) {
        json_t jsonItem(kObjectType);
        JsonUtil::AddMember(jsonItem, "Source", item.source, allocator);
        JsonUtil::AddMember(jsonItem, "Address", item.address, allocator);
        JsonUtil::AddMember(jsonItem, "Ascend Inner Code", item.ascendCInnerCode, allocator);
        JsonUtil::AddMember(jsonItem, "Cycles", item.cycles, allocator);
        JsonUtil::AddMember(jsonItem, "Instructions Executed", item.instructionsExecuted, allocator);
        JsonUtil::AddMember(jsonItem, "Pipe", item.pipe, allocator);
        JsonUtil::AddMember(jsonItem, "RealStallCycles", item.realStallCycles, allocator);
        JsonUtil::AddMember(jsonItem, "TheoreticalStallCycles", item.theoreticalStallCycles, allocator);
        jsonInstructions.PushBack(jsonItem, allocator);
    }
    JsonUtil::AddMember(body, "Instructions", jsonInstructions, allocator);
}

template <>
std::optional<document_t> ToResponseJson<SourceApiInstrDynamicResponse>(const SourceApiInstrDynamicResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    if (response.body.columnNameMap.empty()) { // 如果列名映射表为空，说明是老版本数据
        SetApiInstructionResponseBody(response.body.instructions, body, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
    // 组装表头信息
    json_t jsonInstrDtype(kObjectType);
    json_t jsonInstrColumn(kObjectType);
    for (const auto &item : response.body.columnNameMap) {
        JsonUtil::AddMember(jsonInstrColumn, item.first, item.second, allocator);
    }
    JsonUtil::AddMember(jsonInstrDtype, "Instructions", jsonInstrColumn, allocator);
    // 组装表格内容
    json_t jsonInstructions(kArrayType);
    for (const auto &item : response.body.columnValues) {
        json_t jsonInstruction(kObjectType);
        for (const auto &stringItem : item.stringMap) {
            JsonUtil::AddMember(jsonInstruction, stringItem.first, stringItem.second, allocator);
        }
        for (const auto &intItem : item.intMap) {
            JsonUtil::AddMember(jsonInstruction, intItem.first, intItem.second, allocator);
        }
        for (const auto &floatItem : item.floatMap) {
            JsonUtil::AddMember(jsonInstruction, floatItem.first, floatItem.second, allocator);
        }
        for (const auto &percentageAndDetailsItem : item.percentAndDetailsColumnMap) {
            std::optional<document_t> jsonItem = PercentageAndDetailsToJson(percentageAndDetailsItem.second, allocator);
            JsonUtil::AddMember(jsonInstruction, percentageAndDetailsItem.first, jsonItem.value(), allocator);
        }
        jsonInstructions.PushBack(jsonInstruction, allocator);
    }
    JsonUtil::AddMember(body, "Instructions", jsonInstructions, allocator);
    JsonUtil::AddMember(body, "Instructions Dtype", jsonInstrDtype, allocator);
    // 组装核的名字
    JsonUtil::AddMember(body, "Core", response.body.coreName, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);

    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> PercentageAndDetailsToJson(
    const Dic::Module::Source::PercentageAndDetails &item, Document::AllocatorType &allocator) {
    document_t percentageAndDetailsJson(kObjectType);
    JsonUtil::AddMember(percentageAndDetailsJson, "Percent", item.percentage, allocator);
    json_t detailsJson(kObjectType);
    for (const auto &detail : item.details) {
        JsonUtil::AddMember(detailsJson, detail.first, detail.second, allocator);
    }
    JsonUtil::AddMember(percentageAndDetailsJson, "Details", detailsJson, allocator);
    return std::optional<document_t>(std::move(percentageAndDetailsJson));
}

template <> std::optional<document_t> ToResponseJson<DetailsBaseInfoResponse>(const DetailsBaseInfoResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    std::optional<document_t> jsonCompare = DetailsBaseInfoToJson(response.body.compare, allocator);
    JsonUtil::AddMember(body, "compare", jsonCompare, allocator);
    std::optional<document_t> jsonBaseline = DetailsBaseInfoToJson(response.body.baseline, allocator);
    JsonUtil::AddMember(body, "baseline", jsonBaseline, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> DetailsBaseInfoToJson(
    const DetailsBaseInfoResBody &body, Document::AllocatorType &allocator) {
    document_t bodyJson(kObjectType);
    JsonUtil::AddMember(bodyJson, "name", body.name, allocator);
    JsonUtil::AddMember(bodyJson, "soc", body.soc, allocator);
    JsonUtil::AddMember(bodyJson, "opType", body.opType, allocator);
    JsonUtil::AddMember(bodyJson, "blockDim", body.blockDim, allocator);
    JsonUtil::AddMember(bodyJson, "mixBlockDim", body.mixBlockDim, allocator);
    JsonUtil::AddMember(bodyJson, "duration", body.duration, allocator);
    JsonUtil::AddMember(bodyJson, "deviceId", body.deviceId, allocator);
    JsonUtil::AddMember(bodyJson, "pid", body.pid, allocator);

    json_t blockDetail(kObjectType);
    JsonUtil::AddMember(blockDetail, "headerName", body.blockDetail.headerName, allocator);
    JsonUtil::AddMember(blockDetail, "size", body.blockDetail.size, allocator);
    json_t rowJson(kArrayType);
    for (const auto &rowItem : body.blockDetail.row) {
        json_t oneRow(kObjectType);
        JsonUtil::AddMember(oneRow, "value", rowItem.value, allocator);
        rowJson.PushBack(oneRow, allocator);
    }
    JsonUtil::AddMember(blockDetail, "row", rowJson, allocator);
    JsonUtil::AddMember(bodyJson, "blockDetail", blockDetail, allocator);
    json_t advice(kArrayType);
    for (const auto &item : body.advice) {
        advice.PushBack(json_t().SetString(item.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(bodyJson, "advice", advice, allocator);
    return std::optional<document_t>(std::move(bodyJson));
}

template <> std::optional<document_t> ToResponseJson<DetailsLoadInfoResponse>(const DetailsLoadInfoResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t blockIdList(kArrayType);
    for (const auto &item : response.body.blockIdList) {
        blockIdList.PushBack(json_t().SetString(item.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "blockIdList", blockIdList, allocator);
    std::optional<document_t> chartData = SubBlockDataToJson(response.body.chartData, allocator);
    JsonUtil::AddMember(body, "chartData", chartData, allocator);
    std::optional<document_t> tableData = SubBlockDataToJson(response.body.tableData, allocator);
    JsonUtil::AddMember(body, "tableData", tableData, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> SubBlockDataToJson(const SubBlockData &data, Document::AllocatorType &allocator) {
    document_t chartData(kObjectType);
    json_t detailDataList(kArrayType);
    for (const auto &item : data.detailDataList) {
        json_t compareData(kObjectType);
        std::optional<document_t> compare = SubBlockUnitDataToJson(item.compare, allocator);
        JsonUtil::AddMember(compareData, "compare", compare, allocator);
        std::optional<document_t> baseline = SubBlockUnitDataToJson(item.baseline, allocator);
        JsonUtil::AddMember(compareData, "baseline", baseline, allocator);
        std::optional<document_t> diff = SubBlockUnitDataToJson(item.diff, allocator);
        JsonUtil::AddMember(compareData, "diff", diff, allocator);
        detailDataList.PushBack(compareData, allocator);
    }
    JsonUtil::AddMember(chartData, "detailDataList", detailDataList, allocator);
    json_t advice(kArrayType);
    for (const auto &item : data.advice) {
        advice.PushBack(json_t().SetString(item.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(chartData, "advice", advice, allocator);
    return std::optional<document_t>(std::move(chartData));
}

std::optional<document_t> SubBlockUnitDataToJson(const SubBlockUnitData &data, Document::AllocatorType &allocator) {
    document_t unitData(kObjectType);
    JsonUtil::AddMember(unitData, "blockId", data.blockId, allocator);
    JsonUtil::AddMember(unitData, "blockType", data.blockType, allocator);
    JsonUtil::AddMember(unitData, "name", data.name, allocator);
    JsonUtil::AddMember(unitData, "unit", data.unit, allocator);
    JsonUtil::AddMember(unitData, "value", data.value, allocator);
    JsonUtil::AddMember(unitData, "originValue", data.originValue, allocator);
    return std::optional<document_t>(std::move(unitData));
}

std::vector<std::string> RemoveDuplicateAdvice(const std::vector<std::string> &list) {
    std::vector<std::string> result;
    std::unordered_set<std::string> set;
    for (const auto &item : list) {
        if (set.find(item) == set.end()) {
            set.insert(item);
            result.emplace_back(item);
        }
    }
    return result;
}

template <>
std::optional<document_t> ToResponseJson<DetailsMemoryGraphResponse>(const DetailsMemoryGraphResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t coreMemory(kArrayType);
    for (const auto &item : response.body.coreMemory) {
        json_t singleCoreMemory(kObjectType);
        JsonUtil::AddMember(singleCoreMemory, "advice", RemoveDuplicateAdvice(item.advice), allocator);
        JsonUtil::AddMember(singleCoreMemory, "blockId", item.blockId, allocator);
        json_t l2CacheCompareJson(kObjectType);
        std::optional<document_t> compareCache = L2CacheToJson(item.l2Cache.compare, allocator);
        JsonUtil::AddMember(l2CacheCompareJson, "compare", compareCache, allocator);
        std::optional<document_t> baselineCache = L2CacheToJson(item.l2Cache.baseline, allocator);
        JsonUtil::AddMember(l2CacheCompareJson, "baseline", baselineCache, allocator);
        std::optional<document_t> diffCache = L2CacheToJson(item.l2Cache.diff, allocator);
        JsonUtil::AddMember(l2CacheCompareJson, "diff", diffCache, allocator);
        JsonUtil::AddMember(singleCoreMemory, "l2Cache", l2CacheCompareJson, allocator);
        JsonUtil::AddMember(singleCoreMemory, "blockType", item.blockType, allocator);
        JsonUtil::AddMember(singleCoreMemory, "chipType", item.chipType, allocator);
        json_t memoryUnitJson(kArrayType);
        for (const auto &unit : item.memoryUnit) {
            json_t unitJson(kObjectType);
            std::optional<document_t> compare = MemoryUnitToJson(unit.compare, allocator);
            JsonUtil::AddMember(unitJson, "compare", compare, allocator);
            std::optional<document_t> baseline = MemoryUnitToJson(unit.baseline, allocator);
            JsonUtil::AddMember(unitJson, "baseline", baseline, allocator);
            std::optional<document_t> diff = MemoryUnitToJson(unit.diff, allocator);
            JsonUtil::AddMember(unitJson, "diff", diff, allocator);
            memoryUnitJson.PushBack(unitJson, allocator);
        }
        JsonUtil::AddMember(singleCoreMemory, "memoryUnit", memoryUnitJson, allocator);
        std::optional<document_t> vector = UtilizationRateCompareToJson(item.vector, allocator);
        JsonUtil::AddMember(singleCoreMemory, "vector", vector.value(), allocator);
        std::optional<document_t> vector1 = UtilizationRateCompareToJson(item.vector1, allocator);
        JsonUtil::AddMember(singleCoreMemory, "vector1", vector1.value(), allocator);
        std::optional<document_t> cube = UtilizationRateCompareToJson(item.cube, allocator);
        JsonUtil::AddMember(singleCoreMemory, "cube", cube.value(), allocator);
        coreMemory.PushBack(singleCoreMemory, allocator);
    }
    JsonUtil::AddMember(body, "coreMemory", coreMemory, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> L2CacheToJson(const L2Cache &l2Cache, Document::AllocatorType &allocator) {
    document_t l2CacheJson(kObjectType);
    JsonUtil::AddMember(l2CacheJson, "hitRatio", l2Cache.hitRatio, allocator);
    JsonUtil::AddMember(l2CacheJson, "hit", l2Cache.hit, allocator);
    JsonUtil::AddMember(l2CacheJson, "totalRequest", l2Cache.totalRequest, allocator);
    JsonUtil::AddMember(l2CacheJson, "miss", l2Cache.miss, allocator);
    return std::optional<document_t>(std::move(l2CacheJson));
}

std::optional<document_t> MemoryUnitToJson(const MemoryUnit &memoryUnit, Document::AllocatorType &allocator) {
    document_t unitJson(kObjectType);
    JsonUtil::AddMember(unitJson, "request", memoryUnit.request, allocator);
    JsonUtil::AddMember(unitJson, "requestSuffix", memoryUnit.requestSuffix, allocator);
    JsonUtil::AddMember(unitJson, "display", memoryUnit.display, allocator);
    JsonUtil::AddMember(unitJson, "peakRatio", memoryUnit.peakRatio, allocator);
    JsonUtil::AddMember(unitJson, "bandwidth", memoryUnit.bandwidth, allocator);
    JsonUtil::AddMember(unitJson, "bandwidthSuffix", memoryUnit.bandwidthSuffix, allocator);
    JsonUtil::AddMember(unitJson, "memoryPath", memoryUnit.memoryPath, allocator);
    return std::optional<document_t>(std::move(unitJson));
}

std::optional<document_t> UtilizationRateToJson(const UtilizationRate &rate, Document::AllocatorType &allocator) {
    document_t rateJson(kObjectType);
    JsonUtil::AddMember(rateJson, "cycle", rate.cycle, allocator);
    JsonUtil::AddMember(rateJson, "totalCycles", rate.totalCycles, allocator);
    JsonUtil::AddMember(rateJson, "ratio", rate.ratio, allocator);
    return std::optional<document_t>(std::move(rateJson));
}

std::optional<document_t> UtilizationRateCompareToJson(
    const CompareData<UtilizationRate> &compareRate, Document::AllocatorType &allocator) {
    document_t compareJson(kObjectType);
    std::optional<document_t> compare = UtilizationRateToJson(compareRate.compare, allocator);
    JsonUtil::AddMember(compareJson, "compare", compare, allocator);
    std::optional<document_t> baseline = UtilizationRateToJson(compareRate.baseline, allocator);
    JsonUtil::AddMember(compareJson, "baseline", baseline, allocator);
    std::optional<document_t> diff = UtilizationRateToJson(compareRate.diff, allocator);
    JsonUtil::AddMember(compareJson, "diff", diff, allocator);
    return std::optional<document_t>(std::move(compareJson));
}

template <>
std::optional<document_t> ToResponseJson<DetailsMemoryTableResponse>(const DetailsMemoryTableResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t memoryTable(kArrayType);
    for (const auto &item : response.body.memoryTable) {
        json_t singleMemoryTable(kObjectType);
        JsonUtil::AddMember(singleMemoryTable, "advice", RemoveDuplicateAdvice(item.advice), allocator);
        JsonUtil::AddMember(singleMemoryTable, "blockId", item.blockId, allocator);
        JsonUtil::AddMember(singleMemoryTable, "tableOpType", item.tableOpType, allocator);
        json_t tableDetailListJson(kArrayType);
        for (const auto &tableDetailItem : item.tableDetail) {
            json_t tableDetailJson(kObjectType);
            JsonUtil::AddMember(tableDetailJson, "headerName", tableDetailItem.headerName, allocator);
            JsonUtil::AddMember(tableDetailJson, "tableName", tableDetailItem.tableName, allocator);
            JsonUtil::AddMember(tableDetailJson, "size", tableDetailItem.size, allocator);
            std::optional<document_t> rowJson = CompareTableRowToJson(tableDetailItem.row, allocator);
            JsonUtil::AddMember(tableDetailJson, "row", rowJson, allocator);
            tableDetailListJson.PushBack(tableDetailJson, allocator);
        }
        JsonUtil::AddMember(singleMemoryTable, "tableDetail", tableDetailListJson, allocator);
        memoryTable.PushBack(singleMemoryTable, allocator);
    }
    JsonUtil::AddMember(body, "memoryTable", memoryTable, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> CompareTableRowToJson(
    const std::vector<CompareData<TableRow>> &rows, Document::AllocatorType &allocator) {
    document_t res(kArrayType);
    for (const auto &row : rows) {
        json_t rowJson(kObjectType);
        json_t compareRow(kObjectType);
        JsonUtil::AddMember(compareRow, "name", row.compare.name, allocator);
        JsonUtil::AddMember(compareRow, "value", row.compare.value, allocator);

        json_t baselineRow(kObjectType);
        JsonUtil::AddMember(baselineRow, "name", row.baseline.name, allocator);
        JsonUtil::AddMember(baselineRow, "value", row.baseline.value, allocator);

        json_t diffRow(kObjectType);
        JsonUtil::AddMember(diffRow, "name", row.diff.name, allocator);
        JsonUtil::AddMember(diffRow, "value", row.diff.value, allocator);

        JsonUtil::AddMember(rowJson, "compare", compareRow, allocator);
        JsonUtil::AddMember(rowJson, "baseline", baselineRow, allocator);
        JsonUtil::AddMember(rowJson, "diff", diffRow, allocator);
        res.PushBack(rowJson, allocator);
    }
    return std::optional<document_t>(std::move(res));
}

template <typename T>
void TransformInterCoreLoadDetail(json_t &jDetail, const std::string_view dimensionName, CompareData<T> value,
    int level, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
    json_t jDimension(kObjectType);
    json_t valueJson(kObjectType);
    JsonUtil::AddMember(valueJson, "compare", value.compare, allocator);
    JsonUtil::AddMember(valueJson, "baseline", value.baseline, allocator);
    JsonUtil::AddMember(valueJson, "diff", value.diff, allocator);
    JsonUtil::AddMember(jDimension, "value", valueJson, allocator);
    JsonUtil::AddMember(jDimension, "level", level, allocator);
    JsonUtil::AddMember(jDetail, dimensionName, jDimension, allocator);
}

template <>
std::optional<document_t> ToResponseJson<DetailsInterCoreLoadGraphResponse>(
    const DetailsInterCoreLoadGraphResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "soc", response.body.soc, allocator);
    JsonUtil::AddMember(body, "opType", response.body.opType, allocator);
    JsonUtil::AddMember(body, "advice", response.body.advice, allocator);
    json_t jOpDetails(kArrayType);
    // 转换op detail数组
    for (const auto &opDetail : response.body.opDetails) {
        json_t jOpDetail(kObjectType);
        JsonUtil::AddMember(jOpDetail, "coreId", opDetail.coreId, allocator);
        json_t jSubCoreDetails(kArrayType);
        // 转换 sub core detail数组
        for (const auto &subCoreDetail : opDetail.subCoreDetails) {
            json_t jSubCoreDetail(kObjectType);
            JsonUtil::AddMember(jSubCoreDetail, "subCoreName", subCoreDetail.subCoreName, allocator);
            TransformInterCoreLoadDetail(
                jSubCoreDetail, "cycles", subCoreDetail.cycles.value, subCoreDetail.cycles.level, allocator);
            TransformInterCoreLoadDetail(jSubCoreDetail, "throughput", subCoreDetail.throughput.value,
                subCoreDetail.throughput.level, allocator);
            TransformInterCoreLoadDetail(jSubCoreDetail, "cacheHitRate", subCoreDetail.cacheHitRate.value,
                subCoreDetail.cacheHitRate.level, allocator);
            TransformInterCoreLoadDetail(jSubCoreDetail, "simtVfInstructions", subCoreDetail.simtVfInstructions.value,
                subCoreDetail.simtVfInstructions.level, allocator);
            TransformInterCoreLoadDetail(jSubCoreDetail, "simtVfInstructionPerCycle",
                subCoreDetail.simtVfInstructionPerCycle.value, subCoreDetail.simtVfInstructionPerCycle.level,
                allocator);
            jSubCoreDetails.PushBack(jSubCoreDetail, allocator);
        }
        JsonUtil::AddMember(jOpDetail, "subCoreDetails", jSubCoreDetails, allocator);
        jOpDetails.PushBack(jOpDetail, allocator);
    }
    JsonUtil::AddMember(body, "opDetails", jOpDetails, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);

    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<DetailsRooflineResponse>(const DetailsRooflineResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "soc", response.body.soc, allocator);
    JsonUtil::AddMember(body, "advice", response.body.advice, allocator);
    json_t data(kArrayType);
    for (auto &item : response.body.data) {
        json_t rooflineGraph(kObjectType);
        JsonUtil::AddMember(rooflineGraph, "title", item.title, allocator);
        json_t rooflineList(kArrayType);
        for (auto &roofline : item.rooflines) {
            json_t jsonRoofline(kObjectType);
            JsonUtil::AddMember(jsonRoofline, "bw", roofline.bw, allocator);
            JsonUtil::AddMember(jsonRoofline, "bwName", roofline.bwName, allocator);
            JsonUtil::AddMember(jsonRoofline, "computility", roofline.computility, allocator);
            JsonUtil::AddMember(jsonRoofline, "computilityName", roofline.computilityName, allocator);
            JsonUtil::AddMember(jsonRoofline, "point", roofline.point, allocator);
            JsonUtil::AddMember(jsonRoofline, "ratio", roofline.ratio, allocator);
            rooflineList.PushBack(jsonRoofline, allocator);
        }
        JsonUtil::AddMember(rooflineGraph, "rooflines", rooflineList, allocator);
        data.PushBack(rooflineGraph, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<CachelineRecordResponse>(const CachelineRecordResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    Value bodyValue(kObjectType);

    std::string data = response.body.cachelineRecords;
    if (data.empty()) {
        JsonUtil::AddMember(json, "body", bodyValue, allocator);
        return std::optional<document_t>{std::move(json)};
    }

    document_t document;
    // 处理解析错误
    if (document.Parse(data.c_str()).HasParseError() || !document.IsObject()) {
        JsonUtil::AddMember(json, "body", bodyValue, allocator);
        return std::optional<document_t>{std::move(json)};
    }

    try {
        bodyValue.CopyFrom(document, allocator);
    } catch (const std::exception &e) {
        JsonUtil::AddMember(json, "body", bodyValue, allocator);
        return std::optional<document_t>{std::move(json)};
    }

    JsonUtil::AddMember(json, "body", bodyValue, allocator);
    return std::optional<document_t>{std::move(json)};
}

#pragma endregion
} // end of namespace Protocol
} // end of namespace Dic
