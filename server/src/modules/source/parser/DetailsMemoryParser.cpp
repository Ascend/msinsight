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

#include "unordered_map"
#include "unordered_set"
#include "SourceProtocolResponse.h"
#include "SourceProtocol.h"
#include "BinFileParseUtil.h"
#include "JsonUtil.h"
#include "SafeFile.h"
#include "DetailsMemoryParser.h"

namespace Dic::Module::Source {
using namespace Dic;
using namespace Dic::Server;

bool DetailsMemoryParser::GetDetailsMemoryTable(const std::string &targetBlockId,
    Protocol::DetailsMemoryTableResBody &responseBody, std::string &curFilePath,
    std::map<int, std::vector<Position>> &curBlockMap) {
    if (targetBlockId.empty()) {
        ServerLog::Info("Block id is empty when get details memory table.");
        return true;
    }
    std::ifstream file = OpenReadFileSafely(curFilePath, std::ios::binary);
    std::string memoryTable =
        BinFileParseUtil::GetSingleContentStrByDataType(file, DataTypeEnum::DETAILS_MEMORY_TABLE, curBlockMap);
    file.close();
    if (memoryTable.empty()) {
        ServerLog::Info("Details memory table data does not exist.");
        return true;
    }
    try {
        std::string error;
        auto tableJson = JsonUtil::TryParse<kParseNumbersAsStringsFlag>(memoryTable, error);
        if (!error.empty()) {
            ServerLog::Error("Parse memory graph data error:", error);
            return false;
        }
        if (!tableJson.value().HasMember("table_per_block") || !tableJson.value()["table_per_block"].IsArray()) {
            ServerLog::Error("Memory table data invalid, can not find array member table per block.");
            return false;
        }
        Value &tableList = tableJson.value()["table_per_block"];
        for (const auto &item : tableList.GetArray()) {
            if (targetBlockId != JsonUtil::GetString(item, "block_id")) {
                continue;
            }
            responseBody.memoryTable.push_back(ParseJsonToMemoryTable(item));
        }
        if (responseBody.memoryTable.size() > 1) {
            ServerLog::Warn("Details memory graph data is not unique, block id:", targetBlockId);
        }
        return true;
    } catch (const std::exception &e) {
        ServerLog::Error("Can't parse details memory graph data,not json.Error is ", e.what());
        return false;
    }
}

Protocol::MemoryTable DetailsMemoryParser::ParseJsonToMemoryTable(const json_t &json) {
    Protocol::MemoryTable result;
    result.blockId = JsonUtil::GetString(json, "block_id");
    result.tableOpType = JsonUtil::GetString(json, "table_op_type");
    result.advice = JsonUtil::GetVector<std::string>(json, "advice");
    if (!json.HasMember("table_detail")) {
        ServerLog::Error("Memory table data invalid, table per block lacks table detail member.");
        return result;
    }
    auto &tableDetailJson = const_cast<Value &>(json["table_detail"]);
    if (!tableDetailJson.IsArray()) {
        return result;
    }
    for (auto &item : tableDetailJson.GetArray()) {
        Protocol::TableDetail<Protocol::CompareData<Protocol::TableRow>> tableDetail;
        tableDetail.tableName = JsonUtil::GetString(item, "table_name");
        tableDetail.size = JsonUtil::GetVector<std::string>(item, "size");
        tableDetail.headerName = JsonUtil::GetVector<std::string>(item, "header_name");
        if (!item.HasMember("row")) {
            continue;
        }
        Value &row = item["row"];
        if (!row.IsArray()) {
            continue;
        }
        for (const auto &dataRow : row.GetArray()) {
            Protocol::TableRow memoryTableRow;
            memoryTableRow.name = JsonUtil::GetString(dataRow, "name");
            memoryTableRow.value = JsonUtil::GetVector<std::string>(dataRow, "value");
            Protocol::CompareData<Protocol::TableRow> compareRow;
            compareRow.compare = memoryTableRow;
            tableDetail.row.push_back(compareRow);
        }
        result.tableDetail.push_back(tableDetail);
    }
    return result;
}

bool DetailsMemoryParser::GetDetailsMemoryGraph(const std::string &targetBlockId,
    Protocol::DetailsMemoryGraphResBody &responseBody, std::string &curFilePath,
    std::map<int, std::vector<Position>> &curBlockMap) {
    if (targetBlockId.empty()) {
        ServerLog::Error("Block id is empty when get details memory graph.");
        return true;
    }
    // 读取内存表数据
    std::ifstream file = OpenReadFileSafely(curFilePath, std::ios::binary);
    std::string memoryGraph =
        BinFileParseUtil::GetSingleContentStrByDataType(file, DataTypeEnum::DETAILS_MEMORY_GRAPH, curBlockMap);
    file.close();
    if (memoryGraph.empty()) {
        ServerLog::Info("Details memory graph data does not exist.");
        return true;
    }
    try {
        std::string error;
        auto graphJson = JsonUtil::TryParse<kParseNumbersAsStringsFlag>(memoryGraph, error);
        if (!error.empty()) {
            ServerLog::Error("Parse memory graph data error:", error);
            return false;
        }
        if (!graphJson.value().HasMember("core_memory_map") || !graphJson.value()["core_memory_map"].IsArray()) {
            ServerLog::Error("Memory graph data invalid.");
            return false;
        }
        Value &coreMemoryList = graphJson.value()["core_memory_map"];
        for (const auto &item : coreMemoryList.GetArray()) {
            if (targetBlockId != JsonUtil::GetString(item, "core_no")) {
                continue;
            }
            Protocol::MemoryGraph temp = ParseJsonToMemoryGraph(item);
            responseBody.coreMemory.push_back(temp);
        }
        if (responseBody.coreMemory.size() > 1) {
            ServerLog::Warn("Details memory graph data is not unique, block id:", targetBlockId);
        }
        return true;
    } catch (const std::exception &e) {
        ServerLog::Error("Can't parse details memory graph data,not json.Error is ", e.what());
        return false;
    }
}

Protocol::UtilizationRate DetailsMemoryParser::ParseJsonToUtilizationRate(const json_t &json) {
    Protocol::UtilizationRate utilizationRate;
    utilizationRate.cycle = JsonUtil::GetString(json, "cycle");
    utilizationRate.ratio = JsonUtil::GetString(json, "ratio");
    utilizationRate.totalCycles = JsonUtil::GetString(json, "total_cycles");
    return utilizationRate;
}

Protocol::MemoryGraph DetailsMemoryParser::ParseJsonToMemoryGraph(const json_t &json) {
    Protocol::MemoryGraph temp;
    temp.blockId = JsonUtil::GetString(json, "core_no");
    temp.blockType = JsonUtil::GetString(json, "op_type");
    temp.chipType = JsonUtil::GetString(json, "soc");
    temp.advice = JsonUtil::GetVector<std::string>(json, "advice");

    if (json.HasMember("memory_unit") && json["memory_unit"].IsArray()) {
        auto &memoryUnit = const_cast<Value &>(json["memory_unit"]);
        for (auto &unit : memoryUnit.GetArray()) {
            Protocol::MemoryUnit memoryUnitTemp;
            memoryUnitTemp.memoryPath = JsonUtil::GetString(unit, "memory_path");
            memoryUnitTemp.request = JsonUtil::GetString(unit, "request");
            memoryUnitTemp.requestSuffix = JsonUtil::GetString(unit, "request_suffix");
            memoryUnitTemp.bandwidth = JsonUtil::GetString(unit, "bandwidth");
            memoryUnitTemp.bandwidthSuffix = JsonUtil::GetString(unit, "bandwidth_suffix");
            memoryUnitTemp.peakRatio = JsonUtil::GetString(unit, "peak_ratio");
            memoryUnitTemp.display = (JsonUtil::GetInteger(unit, "display") == 1);
            Protocol::CompareData<Protocol::MemoryUnit> compareData;
            compareData.compare = memoryUnitTemp;
            temp.memoryUnit.push_back(compareData);
        }
    }
    if (json.HasMember("L2cache") && json["L2cache"].IsObject()) {
        auto &L2cache = const_cast<Value &>(json["L2cache"]);
        Protocol::L2Cache l2CacheTemp;
        l2CacheTemp.hit = JsonUtil::GetString(L2cache, "hit");
        l2CacheTemp.miss = JsonUtil::GetString(L2cache, "miss");
        l2CacheTemp.totalRequest = JsonUtil::GetString(L2cache, "total_request");
        l2CacheTemp.hitRatio = JsonUtil::GetString(L2cache, "hit_ratio");
        temp.l2Cache.compare = l2CacheTemp;
    }

    if (json.HasMember("Vector") && json["Vector"].IsObject()) {
        temp.vector.compare = ParseJsonToUtilizationRate(json["Vector"]);
    }
    if (json.HasMember("Vector1") && json["Vector1"].IsObject()) {
        temp.vector1.compare = ParseJsonToUtilizationRate(json["Vector1"]);
    }
    if (json.HasMember("Cube") && json["Cube"].IsObject()) {
        temp.cube.compare = ParseJsonToUtilizationRate(json["Cube"]);
    }
    return temp;
}

bool DetailsMemoryParser::GetDetailsBaseInfo(Protocol::DetailsBaseInfoResBody &responseBody, std::string &curFilePath,
    std::map<int, std::vector<Position>> &curBlockMap) {
    std::ifstream file = OpenReadFileSafely(curFilePath, std::ios::binary);
    if (!file) {
        ServerLog::Error("Open file failed when get details base info.");
        return false;
    }
    std::string baseInfo =
        BinFileParseUtil::GetSingleContentStrByDataType(file, DataTypeEnum::DETAILS_BASE_INFO, curBlockMap);
    file.close();
    if (baseInfo.empty()) {
        ServerLog::Info("Details base info data does not exist.");
        return true;
    }
    try {
        std::string error;
        auto baseInfoJson = JsonUtil::TryParse<kParseNumbersAsStringsFlag>(baseInfo, error);
        if (!error.empty()) {
            ServerLog::Error("Get base info error:", error);
            return false;
        }
        responseBody = ParseJsonToBaseInfo(baseInfoJson.value());
        return true;
    } catch (const std::exception &e) {
        ServerLog::Error("Can't parse details base info,not json.Error is ", e.what());
        return false;
    }
}

Protocol::DetailsBaseInfoResBody DetailsMemoryParser::ParseJsonToBaseInfo(const document_t &json) {
    Protocol::DetailsBaseInfoResBody baseInfoRes;
    baseInfoRes.name = JsonUtil::GetString(json, "name");
    baseInfoRes.soc = JsonUtil::GetString(json, "soc");
    baseInfoRes.opType = JsonUtil::GetString(json, "op_type");
    baseInfoRes.blockDim = JsonUtil::GetString(json, "block_dim");
    baseInfoRes.mixBlockDim = JsonUtil::GetString(json, "mix_block_dim");
    baseInfoRes.duration = JsonUtil::GetString(json, "duration");
    baseInfoRes.deviceId = JsonUtil::GetString(json, "device_id");
    baseInfoRes.pid = JsonUtil::GetString(json, "pid");

    if (!json.HasMember("mix_block_detail") && !json.HasMember("block_detail")) {
        return baseInfoRes;
    }
    const Value &blockDetailsValue = baseInfoRes.opType == "mix" && json.HasMember("mix_block_detail")
        ? json["mix_block_detail"]
        : json["block_detail"];
    if (!blockDetailsValue.IsObject()) {
        return baseInfoRes;
    }
    Protocol::TableDetail<Protocol::TableRow> tableDetail;
    tableDetail.size = JsonUtil::GetVector<std::string>(blockDetailsValue, "size");
    tableDetail.headerName = JsonUtil::GetVector<std::string>(blockDetailsValue, "head_name");
    if (blockDetailsValue.HasMember("row") && blockDetailsValue["row"].IsArray()) {
        const Value &row = blockDetailsValue["row"];
        for (const auto &dataRow : row.GetArray()) {
            Protocol::TableRow memoryTableRow;
            memoryTableRow.value = JsonUtil::GetVector<std::string>(dataRow, "value");
            tableDetail.row.push_back(memoryTableRow);
        }
    }
    baseInfoRes.blockDetail = tableDetail;
    return baseInfoRes;
}

bool DetailsMemoryParser::GetDetailsLoadInfo(Protocol::DetailsLoadInfoResBody &responseBody, std::string &curFilePath,
    std::map<int, std::vector<Position>> &curBlockMap) {
    // 从文件获取内容
    std::ifstream file = OpenReadFileSafely(curFilePath, std::ios::binary);
    if (!file) {
        ServerLog::Error("Open file failed when get details load info.");
        return false;
    }
    std::string loadGraph =
        BinFileParseUtil::GetSingleContentStrByDataType(file, DataTypeEnum::DETAILS_COMPUTE_LOAD_GRAPH, curBlockMap);
    std::string loadTable =
        BinFileParseUtil::GetSingleContentStrByDataType(file, DataTypeEnum::DETAILS_COMPUTE_LOAD_TABLE, curBlockMap);
    file.close();
    if (loadGraph.empty() && loadTable.empty()) {
        ServerLog::Info("Details load data does not exist.");
        return false;
    }

    std::optional<Protocol::SubBlockData> blockData = ConvertStrToSubBlockData(loadGraph);
    if (blockData.has_value()) {
        responseBody.chartData = blockData.value();
    }
    std::optional<Protocol::SubBlockData> tableData = ConvertStrToSubBlockData(loadTable);
    if (tableData.has_value()) {
        responseBody.tableData = tableData.value();
    }

    // 获取blockid列表(以compare数据为准)
    std::unordered_set<std::string> blockIdSet;
    for (const auto &item : responseBody.chartData.detailDataList) {
        blockIdSet.insert(item.compare.blockId);
    }
    for (const auto &item : responseBody.tableData.detailDataList) {
        blockIdSet.insert(item.compare.blockId);
    }
    std::copy(blockIdSet.begin(), blockIdSet.end(), std::back_inserter(responseBody.blockIdList));
    return true;
}

std::optional<Protocol::SubBlockData> DetailsMemoryParser::ConvertStrToSubBlockData(const std::string &str) {
    if (str.empty()) {
        return std::nullopt;
    }
    Protocol::SubBlockData blockData;
    try {
        std::string error;
        auto d = JsonUtil::TryParse<kParseNumbersAsStringsFlag>(str, error);
        if (!error.empty()) {
            ServerLog::Error("Get base info error:", error);
            return std::nullopt;
        }
        blockData.advice = JsonUtil::GetVector<std::string>(d.value(), "advice");
        if (!d.value().HasMember("subblock_detail") || !d.value()["subblock_detail"].IsArray()) {
            ServerLog::Error("Error encountered while converting string to sub-block data"
                             "in the sub-block detail conversion.");
            return std::nullopt;
        }
        Value &blockDetails = d.value()["subblock_detail"];
        std::transform(blockDetails.GetArray().begin(), blockDetails.GetArray().end(),
            std::back_inserter(blockData.detailDataList), ParseSubBlockUnitData);
    } catch (const std::exception &e) {
        ServerLog::Error("Can't convert string to sub block data.Error is ", e.what());
        return std::nullopt;
    }
    return blockData;
}

Protocol::CompareData<Protocol::SubBlockUnitData> DetailsMemoryParser::ParseSubBlockUnitData(const json_t &item) {
    Protocol::SubBlockUnitData unitData;
    unitData.blockId = JsonUtil::GetString(item, "block_id");
    unitData.blockType = JsonUtil::GetString(item, "block_type");
    unitData.name = JsonUtil::GetString(item, "name");
    unitData.unit = GetUnitType(JsonUtil::GetInteger(item, "unit"));
    unitData.value = JsonUtil::GetString(item, "value");
    unitData.originValue = JsonUtil::GetString(item, "origin_value");
    Protocol::CompareData<Protocol::SubBlockUnitData> compareData;
    compareData.compare = unitData;
    return compareData;
}

static inline std::map<int64_t, std::string> unitTypeMapping = {
    {0, "Duration(μs)"}, {1, "Instructions"}, {2, "Data Volume(byte)"}, {3, "PRE"}};

std::string DetailsMemoryParser::GetUnitType(int64_t unitTypeNumber) {
    if (unitTypeMapping.find(unitTypeNumber) != unitTypeMapping.end()) {
        std::string localStr = StringUtil::ToLocalStr(unitTypeMapping[unitTypeNumber]);
        return localStr;
    } else {
        ServerLog::Error("Unknown data block type: ", unitTypeNumber);
        return "";
    }
}

} // Dic
// Module
// Source
