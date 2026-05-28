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
#include "DetailsService.h"
#include "SourceProtocolRequest.h"
#include "SourceProtocolResponse.h"
#include "SourceFileParser.h"
#include "../../TestSuit.h"
#include "BaselineManager.h"

const int NUMBER_ONE = 1;
const int NUMBER_TWO = 2;
const int NUMBER_TWELVE = 12;
const int NUMBER_SIXTEEN = 16;
const int NUMBER_TWENTY = 20;
const int NUMBER_SIXTY_EIGHT = 68;
class DetailsServerTest : public ::testing::Test {
  protected:
    std::string filePath;
    void SetUp() override { filePath = TestSuit::GetTestDataFile("test_bin_mix", "visualize_data.bin"); }
    static Dic::Module::Source::SourceFileParser &InitParser(const std::string &dataPath, const std::string &fileId) {
        Dic::Module::Global::BaselineInfo baselineInfo{"", "baseline", "", ""};
        Dic::Module::Global::BaselineManager::Instance().SetBaselineInfo(baselineInfo);
        Dic::Module::Source::SourceFileParser &parser = Dic::Module::Source::SourceFileParser::Instance();
        std::string errMsg;
        EXPECT_EQ(true, parser.CheckOperatorBinary(dataPath, errMsg));
        if (fileId == "baseline") {
            parser.SetBaselineFilePath(dataPath);
        } else {
            parser.SetFilePath(dataPath);
        }
        parser.Parse(std::vector<std::string>(), fileId, dataPath, "");
        return parser;
    }
};

TEST_F(DetailsServerTest, QueryDetailsLoadInfoWithBaseline) {
    InitParser(filePath, "");
    InitParser(filePath, "baseline");
    Dic::Protocol::SourceDetailsLoadInfoRequest request;
    request.params.isCompared = true;
    Dic::Protocol::DetailsLoadInfoResponse response;
    bool res = Dic::Module::Source::DetailsService::QueryDetailsLoadInfo(request, response);
    EXPECT_EQ(res, true);
    EXPECT_EQ(response.body.chartData.detailDataList.size(), NUMBER_TWELVE);
    CompareData<SubBlockUnitData> compareData;
    for (const auto &item : response.body.chartData.detailDataList) {
        if (item.diff.name == "Vector All Active1") {
            compareData = item;
            break;
        }
    }
    EXPECT_EQ(compareData.diff.name, "Vector All Active1");
    EXPECT_EQ(compareData.diff.value, "0");
    EXPECT_EQ(response.body.tableData.detailDataList.size(), NUMBER_SIXTY_EIGHT);
}

TEST_F(DetailsServerTest, QueryMemoryGraphWithBaseline) {
    InitParser(filePath, "");
    InitParser(filePath, "baseline");
    Dic::Protocol::DetailsMemoryGraphRequest request;
    request.params.isCompared = true;
    request.params.blockId = "0";
    Dic::Protocol::DetailsMemoryGraphResponse response;
    bool res = Dic::Module::Source::DetailsService::QueryMemoryGraph(request, response);
    EXPECT_EQ(res, true);
    EXPECT_EQ(response.body.coreMemory.size(), NUMBER_ONE);
    EXPECT_EQ(response.body.coreMemory[0].blockType, "mix");
    EXPECT_EQ(response.body.coreMemory[0].memoryUnit.size(), NUMBER_TWENTY);
    EXPECT_EQ(response.body.coreMemory[0].l2Cache.baseline.hit, "66845");
}

TEST_F(DetailsServerTest, QueryMemoryTableWithBaseline) {
    InitParser(filePath, "");
    InitParser(filePath, "baseline");
    Dic::Protocol::DetailsMemoryTableRequest request;
    request.params.isCompared = true;
    request.params.blockId = "0";
    Dic::Protocol::DetailsMemoryTableResponse response;
    bool res = Dic::Module::Source::DetailsService::QueryMemoryTable(request, response);
    EXPECT_EQ(res, true);
    EXPECT_EQ(response.body.memoryTable.size(), NUMBER_ONE);
    EXPECT_EQ(response.body.memoryTable[0].tableOpType, "mix");
    EXPECT_EQ(response.body.memoryTable[0].tableDetail.size(), NUMBER_SIXTEEN);
    TableDetail<CompareData<TableRow>> compareData;
    for (const auto &item : response.body.memoryTable[0].tableDetail) {
        if (item.tableName == "Vector Core1") {
            compareData = item;
            break;
        }
    }
    EXPECT_EQ(compareData.row.size(), NUMBER_TWO);
    EXPECT_EQ(compareData.tableName, "Vector Core1");
}

TEST_F(DetailsServerTest, QueryCoreLoadAnalysisGraphWithBaseline) {
    InitParser(filePath, "");
    InitParser(filePath, "baseline");
    Dic::Protocol::DetailsInterCoreLoadGraphRequest request;
    request.params.isCompared = true;
    Dic::Protocol::DetailsInterCoreLoadGraphResponse response;
    bool res = Dic::Module::Source::DetailsService::QueryCoreLoadAnalysisGraph(request, response);
    EXPECT_TRUE(res);
    EXPECT_EQ(response.body.opType, "mix");
    EXPECT_EQ(response.body.soc, "Ascend910B4");
    EXPECT_EQ(response.body.opDetails.size(), NUMBER_TWO);
}
