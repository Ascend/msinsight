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
#include <unordered_map>
#include <algorithm>
#include "SourceFileParser.h"
#include "NumberUtil.h"
#include "DetailsService.h"

namespace Dic {
namespace Module {
namespace Source {

bool DetailsService::QueryDetailsLoadInfo(
    const SourceDetailsLoadInfoRequest &request, DetailsLoadInfoResponse &response) {
    DetailsLoadInfoResBody compareBody;
    bool result = SourceFileParser::Instance().GetDetailsLoadInfo(compareBody, false);
    if (!result) {
        return false;
    }
    // 非对比情况，直接赋值返回
    response.body = compareBody;
    if (!request.params.isCompared) {
        return true;
    }
    // 对比场景,获取baseline数据
    DetailsLoadInfoResBody baselineBody;
    bool baselineRes = SourceFileParser::Instance().GetDetailsLoadInfo(baselineBody, true);
    // baseline数据不存在，直接返回
    if (!baselineRes) {
        return true;
    }
    // baseline数据存在，分别对负载图和负载表数据进行整合
    response.body.chartData = MergeSubBlockData(compareBody.chartData, baselineBody.chartData);
    response.body.tableData = MergeSubBlockData(compareBody.tableData, baselineBody.tableData);
    return true;
}

SubBlockData DetailsService::MergeSubBlockData(const SubBlockData &compare, const SubBlockData &baseline) {
    std::unordered_map<std::string, CompareData<SubBlockUnitData>> subBlockDataMap;
    // 将对比数据存入map中
    for (const auto &item : compare.detailDataList) {
        std::string key = GetSubBlockDataKey(item.compare);
        if (key.empty()) {
            continue;
        }
        subBlockDataMap[key] = item;
        subBlockDataMap[key].baseline.BaseInfoClone(subBlockDataMap[key].compare);
        subBlockDataMap[key].diff.BaseInfoClone(subBlockDataMap[key].compare);
    }
    // 遍历baseline，将baseline数据也存入map中
    for (const auto &item : baseline.detailDataList) {
        std::string key = GetSubBlockDataKey(item.compare);
        if (key.empty()) {
            continue;
        }
        // 如果map中存在对应的对比数据，则计算差异值，对diff内容进行填充
        SubBlockUnitData baselineData = item.compare;
        if (subBlockDataMap.find(key) != subBlockDataMap.end()) {
            // diff数据计算
            SubBlockUnitData compareData = subBlockDataMap[key].compare;
            subBlockDataMap[key].diff.blockId = baselineData.blockId;
            subBlockDataMap[key].diff.name = baselineData.name;
            subBlockDataMap[key].diff.blockType = baselineData.blockType;
            subBlockDataMap[key].diff.unit = baselineData.unit;
            subBlockDataMap[key].diff.value =
                NumberUtil::StringDoubleMinusWithoutTrailingZero(compareData.value, baselineData.value);
            subBlockDataMap[key].diff.originValue =
                NumberUtil::StringDoubleMinusWithoutTrailingZero(compareData.originValue, baselineData.originValue);
            // baseline信息填入（以compare数据为基准）
            subBlockDataMap[key].baseline = baselineData;
        }
    }
    // 将map内容收集并返回合并的结果
    SubBlockData res;
    for (const auto &item : subBlockDataMap) {
        res.detailDataList.push_back(item.second);
    }
    return res;
}

std::string DetailsService::GetSubBlockDataKey(const SubBlockUnitData &data) {
    return data.blockId + underline + data.blockType + data.name;
}

bool DetailsService::QueryMemoryGraph(const DetailsMemoryGraphRequest &request, DetailsMemoryGraphResponse &response) {
    DetailsMemoryGraphResBody compareBody;
    bool result = SourceFileParser::Instance().GetDetailsMemoryGraph(request.params.blockId, false, compareBody);
    if (!result) {
        return false;
    }
    // 非对比情况，直接赋值返回
    response.body = compareBody;
    if (!request.params.isCompared) {
        return true;
    }
    // 对比场景,获取baseline数据
    DetailsMemoryGraphResBody baselineBody;
    bool baselineRes = SourceFileParser::Instance().GetDetailsMemoryGraph(request.params.blockId, true, baselineBody);
    // baseline数据不存在，直接返回
    if (!baselineRes) {
        return true;
    }
    // baseline数据存在，进行内存图中数据的整合
    response.body.coreMemory = MergeMemoryGraph(compareBody.coreMemory, baselineBody.coreMemory);
    return true;
}

std::vector<MemoryGraph> DetailsService::MergeMemoryGraph(
    const std::vector<MemoryGraph> &compare, const std::vector<MemoryGraph> &baseline) {
    std::unordered_map<std::string, MemoryGraph> subMemoryGraphMap;
    for (const auto &item : compare) {
        subMemoryGraphMap[item.blockId] = item;
    }

    for (const auto &item : baseline) {
        // 直接从map中取对应blockId的对象，如果数据不存在，此处会自动新建一个空对象
        MemoryGraph &compareGraph = subMemoryGraphMap[item.blockId];
        if (compareGraph.blockId.empty()) {
            compareGraph.blockId = item.blockId;
            compareGraph.blockType = item.blockType;
            compareGraph.chipType = item.chipType;
        }
        compareGraph.advice = std::vector<std::string>();
        // 分别合并l2cache、vector、vector1、cube块的内容
        compareGraph.l2Cache = MergeL2Cache(compareGraph.l2Cache.compare, item.l2Cache.compare);
        compareGraph.vector = MergeUtilizationRate(compareGraph.vector.compare, item.vector.compare);
        compareGraph.vector1 = MergeUtilizationRate(compareGraph.vector1.compare, item.vector1.compare);
        compareGraph.cube = MergeUtilizationRate(compareGraph.cube.compare, item.cube.compare);
        compareGraph.memoryUnit = MergeMemoryUnit(compareGraph.memoryUnit, item.memoryUnit);
    }

    std::vector<MemoryGraph> res;
    res.reserve(subMemoryGraphMap.size());
    for (const auto &item : subMemoryGraphMap) {
        res.push_back(item.second);
    }
    return res;
}

std::vector<CompareData<MemoryUnit>> DetailsService::MergeMemoryUnit(
    const std::vector<CompareData<MemoryUnit>> &compare, const std::vector<CompareData<MemoryUnit>> &baseline) {
    std::unordered_map<std::string, CompareData<MemoryUnit>> memoryUnitMap;
    for (const auto &item : compare) {
        memoryUnitMap[item.compare.memoryPath] = item;
    }
    for (const auto &item : baseline) {
        MemoryUnit baselineMemoryUnit = item.compare;
        if (memoryUnitMap.find(baselineMemoryUnit.memoryPath) != memoryUnitMap.end()) {
            MemoryUnit compareMemoryUnit = memoryUnitMap[baselineMemoryUnit.memoryPath].compare;
            MemoryUnit diff;
            diff.memoryPath = baselineMemoryUnit.memoryPath;
            diff.request =
                NumberUtil::StringUnsignedLongLongMinus(compareMemoryUnit.request, baselineMemoryUnit.request);
            diff.requestSuffix = compareMemoryUnit.requestSuffix;
            diff.bandwidth = NumberUtil::StringDoubleMinusWithoutTrailingZero(
                compareMemoryUnit.bandwidth, baselineMemoryUnit.bandwidth);
            diff.bandwidthSuffix = compareMemoryUnit.bandwidthSuffix;
            diff.peakRatio = NumberUtil::StringDoubleMinusWithoutTrailingZero(
                compareMemoryUnit.peakRatio, baselineMemoryUnit.peakRatio);
            // 线的数据是否展示以compare数据为准
            diff.display = compareMemoryUnit.display;
            memoryUnitMap[baselineMemoryUnit.memoryPath].diff = diff;
        }
        memoryUnitMap[baselineMemoryUnit.memoryPath].baseline = baselineMemoryUnit;
    }
    std::vector<CompareData<MemoryUnit>> res;
    res.reserve(memoryUnitMap.size());
    for (const auto &item : memoryUnitMap) {
        res.push_back(item.second);
    }
    return res;
}

CompareData<Protocol::L2Cache> DetailsService::MergeL2Cache(const L2Cache &compare, const L2Cache &baseline) {
    CompareData<L2Cache> res;
    res.compare = compare;
    res.baseline = baseline;
    res.diff.totalRequest =
        NumberUtil::StringDoubleMinusWithoutTrailingZero(res.compare.totalRequest, res.baseline.totalRequest);
    res.diff.miss = NumberUtil::StringDoubleMinusWithoutTrailingZero(res.compare.miss, res.baseline.miss);
    res.diff.hit = NumberUtil::StringDoubleMinusWithoutTrailingZero(res.compare.hit, res.baseline.hit);
    res.diff.hitRatio = NumberUtil::StringDoubleMinusWithoutTrailingZero(res.compare.hitRatio, res.baseline.hitRatio);
    return res;
}

CompareData<UtilizationRate> DetailsService::MergeUtilizationRate(
    const UtilizationRate &compare, const UtilizationRate &baseline) {
    CompareData<UtilizationRate> res;
    res.compare = compare;
    res.baseline = baseline;
    res.diff.ratio = NumberUtil::StringDoubleMinusWithoutTrailingZero(res.compare.ratio, res.baseline.ratio);
    res.diff.totalCycles =
        NumberUtil::StringDoubleMinusWithoutTrailingZero(res.compare.totalCycles, res.baseline.totalCycles);
    res.diff.cycle = NumberUtil::StringDoubleMinusWithoutTrailingZero(res.compare.cycle, res.baseline.cycle);
    return res;
}

bool DetailsService::QueryMemoryTable(const DetailsMemoryTableRequest &request, DetailsMemoryTableResponse &response) {
    DetailsMemoryTableResBody compareBody;
    bool result = SourceFileParser::Instance().GetDetailsMemoryTable(request.params.blockId, false, compareBody);
    if (!result) {
        return false;
    }
    // 非对比情况，直接赋值返回
    response.body = compareBody;
    if (!request.params.isCompared) {
        return true;
    }
    // 对比场景,获取baseline数据
    DetailsMemoryTableResBody baselineBody;
    bool baselineRes = SourceFileParser::Instance().GetDetailsMemoryTable(request.params.blockId, true, baselineBody);
    // baseline数据不存在，直接返回
    if (!baselineRes) {
        return true;
    }
    // baseline数据存在，进行数据整合
    response.body.memoryTable = MergeMemoryTables(compareBody.memoryTable, baselineBody.memoryTable);
    return true;
}

std::vector<MemoryTable> DetailsService::MergeMemoryTables(
    const std::vector<MemoryTable> &compare, const std::vector<MemoryTable> &baseline) {
    std::unordered_map<std::string, MemoryTable> memoryTableMap;
    for (const auto &item : compare) {
        memoryTableMap[item.blockId] = item;
        memoryTableMap[item.blockId].FillBaseInfoFromCompare();
    }
    for (const auto &item : baseline) {
        MemoryTable &table = memoryTableMap[item.blockId];
        if (table.blockId.empty()) {
            table.blockId = item.blockId;
            table.advice = std::vector<std::string>();
            table.tableOpType = item.tableOpType;
        }
        table.tableDetail = MergeCompareTableList(table.tableDetail, item.tableDetail);
    }
    std::vector<MemoryTable> res;
    res.reserve(memoryTableMap.size());
    for (const auto &item : memoryTableMap) {
        res.push_back(item.second);
    }
    return res;
}

std::vector<TableDetail<CompareData<TableRow>>> DetailsService::MergeCompareTableList(
    const std::vector<TableDetail<CompareData<TableRow>>> &compare,
    const std::vector<TableDetail<CompareData<TableRow>>> &baseline) {
    std::unordered_map<std::string, TableDetail<CompareData<TableRow>>> memoryTableDetailMap;
    for (const auto &item : compare) {
        memoryTableDetailMap[item.tableName] = item;
    }

    for (const auto &item : baseline) {
        // 以compare的数据为准，如果compare有数据 baseline没数据，则不显示
        if (memoryTableDetailMap.count(item.tableName) == 0) {
            continue;
        }
        TableDetail<CompareData<TableRow>> &tableDetail = memoryTableDetailMap[item.tableName];
        tableDetail.row = MergeCompareRows(memoryTableDetailMap[item.tableName].row, item.row);
        // 表格内容合并后，重置表格大小
        std::vector<std::string> newSize = {
            std::to_string(tableDetail.headerName.size()), std::to_string(tableDetail.row.size())};
        tableDetail.size = newSize;
    }
    std::vector<TableDetail<CompareData<TableRow>>> res;
    res.reserve(memoryTableDetailMap.size());
    for (const auto &item : memoryTableDetailMap) {
        res.push_back(item.second);
    }
    return res;
}

std::vector<CompareData<TableRow>> DetailsService::MergeCompareRows(
    const std::vector<CompareData<TableRow>> &compare, const std::vector<CompareData<TableRow>> &baseline) {
    std::vector<CompareData<TableRow>> res;
    std::unordered_map<std::string, CompareData<TableRow>> tableRowMap;
    for (const auto &item : compare) {
        tableRowMap[item.compare.name] = item;
    }

    for (const auto &item : baseline) {
        std::string rowName = item.compare.name;
        if (tableRowMap.find(rowName) != tableRowMap.end()) {
            unsigned long long colNum = std::min(tableRowMap[rowName].compare.value.size(), item.compare.value.size());
            for (unsigned long long i = 0; i < colNum; ++i) {
                tableRowMap[rowName].diff.value[i] = NumberUtil::StringDoubleMinusWithoutTrailingZero(
                    tableRowMap[rowName].compare.value[i], item.compare.value[i]);
            }
            // baseline数据填充(以compare数据为准)
            tableRowMap[rowName].baseline = item.compare;
        }
    }

    res.reserve(tableRowMap.size());
    for (const auto &item : tableRowMap) {
        res.push_back(item.second);
    }
    return res;
}

bool DetailsService::QueryCoreLoadAnalysisGraph(
    const DetailsInterCoreLoadGraphRequest &request, DetailsInterCoreLoadGraphResponse &response) {
    DetailsInterCoreLoadGraphBody compareBody;
    bool result = SourceFileParser::Instance().GetDetailsInterCoreLoadAnalysisGraph(compareBody, false);
    if (!result) {
        return false;
    }
    // 非对比情况，直接赋值返回
    response.body = compareBody;
    if (!request.params.isCompared) {
        return true;
    }
    // 对比场景,获取baseline数据
    DetailsInterCoreLoadGraphBody baselineBody;
    bool baselineRes = SourceFileParser::Instance().GetDetailsInterCoreLoadAnalysisGraph(baselineBody, true);
    // baseline数据不存在，直接返回
    if (!baselineRes) {
        return true;
    }
    // baseline数据存在，进行数据整合
    response.body.opDetails = MergeCoreLoadOpDetail(compareBody.opDetails, baselineBody.opDetails);
    return true;
}

std::vector<DetailsInterCoreLoadOpDetail> DetailsService::MergeCoreLoadOpDetail(
    const std::vector<DetailsInterCoreLoadOpDetail> &compare,
    const std::vector<DetailsInterCoreLoadOpDetail> &baseline) {
    std::unordered_map<uint8_t, DetailsInterCoreLoadOpDetail> opDetailMap;
    for (const auto &item : compare) {
        opDetailMap[item.coreId] = item;
    }
    for (const auto &item : baseline) {
        // 这里会查找compare中是否存在数据，如果存在才会记录baseline的信息，否则不对这条数据进行合并
        if (opDetailMap.find(item.coreId) == opDetailMap.end()) {
            continue;
        }
        opDetailMap[item.coreId].subCoreDetails =
            MergeCoreDetail(opDetailMap[item.coreId].subCoreDetails, item.subCoreDetails);
    }

    std::vector<DetailsInterCoreLoadOpDetail> result;
    result.reserve(opDetailMap.size());
    for (const auto &item : opDetailMap) {
        result.push_back(item.second);
    }
    return result;
}

std::vector<DetailsInterCoreLoadSubCoreDetail> DetailsService::MergeCoreDetail(
    const std::vector<DetailsInterCoreLoadSubCoreDetail> &compare,
    const std::vector<DetailsInterCoreLoadSubCoreDetail> &baseline) {
    std::unordered_map<std::string, DetailsInterCoreLoadSubCoreDetail> coreDetailMap;
    for (const auto &item : compare) {
        coreDetailMap[item.subCoreName] = item;
    }

    for (const auto &item : baseline) {
        if (coreDetailMap.find(item.subCoreName) == coreDetailMap.end()) {
            continue;
        }
        DetailsInterCoreLoadSubCoreDetail &detail = coreDetailMap[item.subCoreName];
        detail.cycles.value.baseline = item.cycles.value.compare;
        detail.cycles.value.diff = NumberSafe::Sub(item.cycles.value.compare, detail.cycles.value.baseline);
        detail.throughput.value.baseline = item.throughput.value.compare;
        detail.throughput.value.diff = NumberSafe::Sub(item.throughput.value.compare, detail.throughput.value.baseline);
        detail.cacheHitRate.value.baseline = item.cacheHitRate.value.compare;
        detail.cacheHitRate.value.diff =
            NumberSafe::Sub(item.cacheHitRate.value.compare, detail.cacheHitRate.value.baseline);
    }
    std::vector<DetailsInterCoreLoadSubCoreDetail> result;
    result.reserve(coreDetailMap.size());
    for (const auto &item : coreDetailMap) {
        result.push_back(item.second);
    }
    return result;
}
}
}
}
