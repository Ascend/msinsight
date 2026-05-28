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

#include <gtest/gtest.h>
#include <cstdint>
#include "ProtocolManager.h"
#include "SourceProtocolResponse.h"
#include "SourceProtocolTest.h"

using namespace Dic::Protocol;

class SourceProtocolTest : public ::testing::Test {
  protected:
    ProtocolManager *manager;
    std::string error;

  protected:
    void SetUp() override { manager = &ProtocolManager::Instance(); }
};

TEST_F(SourceProtocolTest, ToCodeFileRequest) {
    const std::unique_ptr<Request> &ptr = manager->FromJson(TO_CODE_FILE_REQ_JSON, error);
    EXPECT_EQ(ptr->moduleName, MODULE_SOURCE);
}

TEST_F(SourceProtocolTest, ToApiLineRequest) {
    const std::unique_ptr<Request> &ptr = manager->FromJson(TO_API_LINE_REQ_JSON, error);
    EXPECT_EQ(ptr->moduleName, MODULE_SOURCE);
}

TEST_F(SourceProtocolTest, ToApiInstrRequest) {
    const std::unique_ptr<Request> &ptr = manager->FromJson(TO_API_INSTR_REQ_JSON, error);
    EXPECT_EQ(ptr->moduleName, MODULE_SOURCE);
}

TEST_F(SourceProtocolTest, ToDetailsBaseInfoRequest) {
    const std::unique_ptr<Request> &ptr = manager->FromJson(TO_BASE_INFO_REQ_JSON, error);
    EXPECT_EQ(ptr->moduleName, MODULE_SOURCE);
}

TEST_F(SourceProtocolTest, ToDetailsLoadInfoRequest) {
    const std::unique_ptr<Request> &ptr = manager->FromJson(TO_LOAD_INFO_REQ_JSON, error);
    EXPECT_EQ(ptr->moduleName, MODULE_SOURCE);
}

TEST_F(SourceProtocolTest, ToDetailsMemoryGraphRequest) {
    const std::unique_ptr<Request> &ptr = manager->FromJson(TO_MEMORY_GRAPH_REQ_JSON, error);
    EXPECT_EQ(ptr->moduleName, MODULE_SOURCE);
}

TEST_F(SourceProtocolTest, ToDetailsMemoryTableRequest) {
    const std::unique_ptr<Request> &ptr = manager->FromJson(TO_MEMORY_TABLE_REQ_JSON, error);
    EXPECT_EQ(ptr->moduleName, MODULE_SOURCE);
}

TEST_F(SourceProtocolTest, ToDetailsInterCoreLoadGraphRequest) {
    const std::unique_ptr<Request> &ptr = manager->FromJson(TO_INTER_CORE_LOAD_GRAPH_JSON, error);
    EXPECT_EQ(ptr->moduleName, MODULE_SOURCE);
}

TEST_F(SourceProtocolTest, ToCodeFileResponse) {
    SourceCodeFileResponse response;
    response.moduleName = MODULE_SOURCE;
    manager->ToJson(response, error);
}

TEST_F(SourceProtocolTest, ToApiLineResponse) {
    SourceApiLineResponse response;
    SourceFileLineRes lineRes;
    std::pair<std::string, std::string> pair = {"1", "10"};
    lineRes.addressRange.emplace_back(pair);
    response.body.lines.emplace_back(lineRes);
    response.moduleName = MODULE_SOURCE;
    manager->ToJson(response, error);
}

TEST_F(SourceProtocolTest, ToApiInstrResponse) {
    SourceApiInstrResponse response;
    response.moduleName = MODULE_SOURCE;
    manager->ToJson(response, error);
}

TEST_F(SourceProtocolTest, ToDetailsBaseInfoResponse) {
    DetailsBaseInfoResponse response;
    response.moduleName = MODULE_SOURCE;
    manager->ToJson(response, error);
}

TEST_F(SourceProtocolTest, ToDetailsLoadInfoResponse) {
    DetailsLoadInfoResponse response;
    SubBlockUnitData subBlockUnitData;
    CompareData<SubBlockUnitData> blockCompareData;
    blockCompareData.compare = subBlockUnitData;
    response.body.chartData.detailDataList.emplace_back(blockCompareData);
    response.body.tableData.detailDataList.emplace_back(blockCompareData);
    response.moduleName = MODULE_SOURCE;
    manager->ToJson(response, error);
}

TEST_F(SourceProtocolTest, ToDetailsMemoryGraphResponse) {
    DetailsMemoryGraphResponse response;
    MemoryGraph memoryGraph;
    MemoryUnit memoryUnit;
    CompareData<MemoryUnit> memoryCompareData;
    memoryCompareData.compare = memoryUnit;
    memoryGraph.memoryUnit.emplace_back(memoryCompareData);
    response.body.coreMemory.emplace_back(memoryGraph);
    response.moduleName = MODULE_SOURCE;
    manager->ToJson(response, error);
}

TEST_F(SourceProtocolTest, ToDetailsMemoryTableResponse) {
    DetailsMemoryTableResponse response;
    MemoryTable memoryTable;
    TableDetail<CompareData<TableRow>> tableDetail;
    CompareData<TableRow> tableRowCompare;
    TableRow tableRow;
    tableRowCompare.compare = tableRow;
    tableDetail.row.emplace_back(tableRowCompare);
    memoryTable.tableDetail.emplace_back(tableDetail);
    response.body.memoryTable.emplace_back(memoryTable);
    response.moduleName = MODULE_SOURCE;
    manager->ToJson(response, error);
}

TEST_F(SourceProtocolTest, ToDetailsInterCoreLoadGraphResponse) {
    DetailsInterCoreLoadGraphResponse response;
    DetailsInterCoreLoadOpDetail opDetail;
    DetailsInterCoreLoadSubCoreDetail subCoreDetail;
    float curRate = 11.0f;
    float maxRate = 23.0f;
    subCoreDetail.SetCacheHitRateDimension(curRate, maxRate, 0);
    uint64_t curCycles = 100;
    subCoreDetail.SetCyclesDimension(curCycles, 0, 0);
    subCoreDetail.SetThroughputDimension(0, 0, 0);
    uint8_t subCoreIndex = 0;
    subCoreDetail.SetSubCoreName("cube", subCoreIndex);
    opDetail.AddSubCoreDetail(std::move(subCoreDetail));
    response.body.opDetails.emplace_back(opDetail);
    response.moduleName = MODULE_SOURCE;
    manager->ToJson(response, error);
}
