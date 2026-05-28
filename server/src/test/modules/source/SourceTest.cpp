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
#include "SourceFileParser.h"
#include "SourceProtocolRequest.h"
#include "mockUtils/BinFileGenerator.h"
#include "DetailsComputeLoadJson.h"
#include "DetailsMemoryJson.h"
#include "DetailsRoofLineJson.h"
#include "SourceTest.h"

namespace {
std::string g_apiFileData = "apiFileData.bin";
std::string g_loadData = "memory_data.bin";
std::string g_memoryData = "base_info.bin";
std::string g_baseInfo = "load_data.bin";
std::string g_rooflineDataFile = "roofline_data.bin";
}

using namespace std;
using namespace Dic;
using namespace Dic::Module::Source::Test;
class SourceTest : public ::testing::Test {
  protected:
    static void SetUpTestCase() {
        BinFileGenerator dataFileGenerator;
        // add source code
        SourceDataBlock sourceDataBlock1(FOREACH_TILLING_DEF_H, FOREACH_TILLING_DEF_H_PATH);
        dataFileGenerator.AddDataBlock(std::make_unique<SourceDataBlock>(sourceDataBlock1));
        SourceDataBlock sourceDataBlock2(KERNEL_FOREACH_BASE_H, KERNEL_FOREACH_BASE_H_PATH);
        dataFileGenerator.AddDataBlock(std::make_unique<SourceDataBlock>(sourceDataBlock2));
        SourceDataBlock sourceDataBlock3(FOREACH_SUB_SCALAR_LIST_CPP, FOREACH_SUB_SCALAR_LIST_CPP_PATH);
        dataFileGenerator.AddDataBlock(std::make_unique<SourceDataBlock>(sourceDataBlock3));
        // add api instructions
        NormalDataBlock apiInstrDataBlock(DataTypeEnum::API_INSTR, API_INSTR_JSON);
        dataFileGenerator.AddDataBlock(std::make_unique<NormalDataBlock>(apiInstrDataBlock));
        // add api file
        NormalDataBlock apiFileDataBlock(DataTypeEnum::API_FILE, API_FILE_JSON);
        dataFileGenerator.AddDataBlock(std::make_unique<NormalDataBlock>(apiFileDataBlock));
        // generate file
        dataFileGenerator.Generate(g_apiFileData);

        BinFileGenerator memoryDataFileGenerator;
        // add memory graph json
        NormalDataBlock memoryGraphDataBlock(DataTypeEnum::DETAILS_MEMORY_GRAPH, DETAILS_MEMORY_GRAPH_JSON);
        memoryDataFileGenerator.AddDataBlock(std::make_unique<NormalDataBlock>(memoryGraphDataBlock));
        // add memory table json
        NormalDataBlock memoryTableDataBlock(DataTypeEnum::DETAILS_MEMORY_TABLE, DETAILS_MEMORY_TABLE_JSON);
        memoryDataFileGenerator.AddDataBlock(std::make_unique<NormalDataBlock>(memoryTableDataBlock));
        // generate file
        memoryDataFileGenerator.Generate(g_memoryData);

        BinFileGenerator baseInfoGenerator;
        // add base info json
        NormalDataBlock baseInfoDataBlock(DataTypeEnum::DETAILS_BASE_INFO, DETAILS_BASE_INFO_JSON);
        baseInfoGenerator.AddDataBlock(std::make_unique<NormalDataBlock>(baseInfoDataBlock));
        // generate file
        baseInfoGenerator.Generate(g_baseInfo);

        BinFileGenerator loadDataGenerator;
        // add compute load graph json
        NormalDataBlock computeLoadGraphDataBlock(
            DataTypeEnum::DETAILS_COMPUTE_LOAD_GRAPH, DETAILS_COMPUTE_LOAD_GRAPH_JSON);
        loadDataGenerator.AddDataBlock(std::make_unique<NormalDataBlock>(computeLoadGraphDataBlock));
        // add compute load table json
        NormalDataBlock computeLoadTableDataBlock(
            DataTypeEnum::DETAILS_COMPUTE_LOAD_TABLE, DETAILS_COMPUTE_LOAD_TABLE_JSON);
        loadDataGenerator.AddDataBlock(std::make_unique<NormalDataBlock>(computeLoadTableDataBlock));
        // generate file
        loadDataGenerator.Generate(g_loadData);

        BinFileGenerator roofLineDataGenerator;
        // add roofline data json
        NormalDataBlock roofLineDataBaseInfo(DataTypeEnum::DETAILS_BASE_INFO, ROOFLINE_DATA_BASE_INFO_JSON);
        NormalDataBlock roofLineData(DataTypeEnum::DETAILS_ROOFLINE, ROOFLINE_DATA_JSON);
        roofLineDataGenerator.AddDataBlock(std::make_unique<NormalDataBlock>(roofLineDataBaseInfo));
        roofLineDataGenerator.AddDataBlock(std::make_unique<NormalDataBlock>(roofLineData));
        roofLineDataGenerator.Generate(g_rooflineDataFile);
    }

    static void TearDownTestCase() {
        // remove temprary bin files
        BinFileGenerator::RemoveFile(g_apiFileData);
        BinFileGenerator::RemoveFile(g_loadData);
        BinFileGenerator::RemoveFile(g_memoryData);
        BinFileGenerator::RemoveFile(g_baseInfo);
        BinFileGenerator::RemoveFile(g_rooflineDataFile);
    }
};

TEST_F(SourceTest, CheckOperatorBinarySuccessfulWithFixedPath) {
    Module::Source::SourceFileParser &parser = Dic::Module::Source::SourceFileParser::Instance();
    std::string errMsg;
    EXPECT_EQ(true, parser.CheckOperatorBinary(g_apiFileData, errMsg));
    EXPECT_EQ(true, parser.CheckOperatorBinary(g_memoryData, errMsg));
    EXPECT_EQ(true, parser.CheckOperatorBinary(g_baseInfo, errMsg));
}

TEST_F(SourceTest, CheckOperatorBinaryFailedWithSpecificPath) {
    Module::Source::SourceFileParser &parser = Dic::Module::Source::SourceFileParser::Instance();
#ifdef _WIN32
    const int pathLen = MAX_PATH;
#else
    const int pathLen = PATH_MAX;
#endif
    string path;
    const int filenameMaxLen = 255;
    while (path.length() < pathLen - 1) {
        path += "0";
        if (path.length() % filenameMaxLen == 0) {
            path += "/";
        }
    }
    std::string errMsg;
    // Failed to check file exists
    EXPECT_EQ(false, parser.CheckOperatorBinary(path, errMsg));

    // Failed to check file length
    path += "0";
    EXPECT_EQ(false, parser.CheckOperatorBinary(path, errMsg));
}

static Module::Source::SourceFileParser &InitParser(const string &dataPath) {
    Module::Source::SourceFileParser &parser = Dic::Module::Source::SourceFileParser::Instance();
    std::string errMsg;
    EXPECT_EQ(true, parser.CheckOperatorBinary(dataPath, errMsg));
    parser.SetFilePath(dataPath);
    parser.Parse(std::vector<std::string>(), "", dataPath, "");
    return parser;
}

static void CleanParser(Module::Source::SourceFileParser &parser) { parser.Reset(); }

TEST_F(SourceTest, GetCoreList) {
    auto &parser = InitParser(g_apiFileData);
    const vector<std::string> &coreList = parser.GetCoreList();
    int coreListSize = 3;
    EXPECT_EQ(coreList.size(), coreListSize);
    EXPECT_EQ(coreList[0], "core0.veccore0");
    EXPECT_EQ(coreList[1], "core0.veccore1");
    EXPECT_EQ(coreList[2], "core1.veccore0"); // the core index is 2
    CleanParser(parser);
}

TEST_F(SourceTest, GetSourceList) {
    auto &parser = InitParser(g_apiFileData);
    const vector<std::string> &sourceList = parser.GetSourceList();
    int sourceListSize = 3;
    EXPECT_EQ(sourceList.size(), sourceListSize);
    EXPECT_EQ(sourceList[0], FOREACH_TILLING_DEF_H_PATH);
    EXPECT_EQ(sourceList[1], KERNEL_FOREACH_BASE_H_PATH);
    EXPECT_EQ(sourceList[2], FOREACH_SUB_SCALAR_LIST_CPP_PATH); // index of scalar list cpp is 2
    CleanParser(parser);
}

TEST_F(SourceTest, GetApiLinesByCoreAndSource) {
    auto &parser = InitParser(g_apiFileData);
    vector<Dic::Module::Source::SourceFileLine> apiLines = parser.GetApiLinesByCoreAndSource(
        "core0.cubecore", "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h");
    EXPECT_EQ(apiLines.size(), 0);

    apiLines = parser.GetApiLinesByCoreAndSource(
        "core0.veccore0", "/home/test/workspace/foreach/common/foreach/foreach_tiling.h");
    EXPECT_EQ(apiLines.size(), 0);

    const vector<Dic::Module::Source::SourceFileLine> &linesByCoreAndSource = parser.GetApiLinesByCoreAndSource(

        "core0.veccore0", "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h");
    int linesByCoreAndSourceSize = 2;
    EXPECT_EQ(linesByCoreAndSource.size(), linesByCoreAndSourceSize);
    EXPECT_EQ(31, linesByCoreAndSource[0].line); // the line is 31
    EXPECT_EQ(15674, linesByCoreAndSource[0].cycles[0]); // the size of cycles is 15674
    EXPECT_EQ(192, linesByCoreAndSource[0].instructionsExecuted[0]); // the size of instructions is 192
    EXPECT_EQ("0x11244240", linesByCoreAndSource[0].addressRange[0].first);
    EXPECT_EQ("0x11244240", linesByCoreAndSource[0].addressRange[0].second);

    EXPECT_EQ(32, linesByCoreAndSource[1].line); // the size of line is 32 and the index is 1
    EXPECT_EQ(16293, linesByCoreAndSource[1].cycles[0]); // the size of cycles is 16293
    EXPECT_EQ(193, linesByCoreAndSource[1].instructionsExecuted[0]); // the size of instructions is 193
    EXPECT_EQ("0x11244540", linesByCoreAndSource[1].addressRange[0].first); // the index of line is 1
    EXPECT_EQ("0x11244540", linesByCoreAndSource[1].addressRange[0].second); // the index of line is 1
    EXPECT_EQ("0x11244548", linesByCoreAndSource[1].addressRange[1].first); // the index of line is 1
    EXPECT_EQ("0x11244844", linesByCoreAndSource[1].addressRange[1].second); // the index of line is 1
    CleanParser(parser);
}

TEST_F(SourceTest, GetInstr) {
    auto &parser = InitParser(g_apiFileData);
    const string &instr = parser.GetInstr();
    EXPECT_TRUE(!instr.empty());
    EXPECT_EQ(true, StringUtil::Contains(instr, "Cores"));
    EXPECT_EQ(true, StringUtil::Contains(instr, "core0.veccore0"));
    EXPECT_EQ(true, StringUtil::Contains(instr, "Address"));
    EXPECT_EQ(true, StringUtil::Contains(instr, "Instructions Executed"));
    CleanParser(parser);
}

TEST_F(SourceTest, GetSourceByName) {
    auto &parser = InitParser(g_apiFileData);
    std::string foreachTillingPath = "/home/test/workspace/foreach/common/foreach/foreach_tiling.h";
    const string notExistsource = parser.GetSourceByName(foreachTillingPath);
    EXPECT_TRUE(notExistsource.empty());

    std::string foreachTillingDefPath = "/home/test/workspace/foreach/common/foreach/foreach_tiling_def.h";
    const string &source = parser.GetSourceByName(foreachTillingDefPath);
    EXPECT_TRUE(!source.empty());

    EXPECT_EQ(true, StringUtil::Contains(source, "__FOREACH_TILING_DEF_H__"));
    EXPECT_EQ(true, StringUtil::Contains(source, "ForeachCommonTilingData"));
    EXPECT_EQ(true, StringUtil::Contains(source, "endif"));
    CleanParser(parser);
}

TEST_F(SourceTest, QueryBaseInfo) {
    auto &parser = InitParser(g_baseInfo);
    Protocol::DetailsBaseInfoResBody resBody;
    bool res = parser.GetDetailsBaseInfo(resBody, false);
    EXPECT_EQ(true, res);
    EXPECT_EQ("sin_custom", resBody.name);
    EXPECT_EQ("Ascend910B4", resBody.soc);
    EXPECT_EQ("8", resBody.blockDim);
    EXPECT_EQ("13.0600004196167", resBody.duration);
    parser.Reset();
}

TEST_F(SourceTest, QueryBaseInfoWhenOpTypeIsMix) {
    auto &parser = InitParser(g_rooflineDataFile);
    Protocol::DetailsBaseInfoResBody resBody;
    bool res = parser.GetDetailsBaseInfo(resBody, false);
    EXPECT_EQ(true, res);
    EXPECT_EQ("gen_FlashAttentionScore_10000000012201130953_mix_aic", resBody.name);
    EXPECT_EQ("Ascend910B4", resBody.soc);
    EXPECT_EQ("4", resBody.blockDim);
    EXPECT_EQ("mix", resBody.opType);
    EXPECT_EQ("301.0400085449219", resBody.duration);
    EXPECT_EQ(2, resBody.blockDetail.size.size()); // the size of size is 2
    EXPECT_EQ(4, resBody.blockDetail.row.size()); // the size of row is 4
    parser.Reset();
}

TEST_F(SourceTest, GetDetailsLoadInfo) {
    auto &parser = InitParser(g_loadData);
    Protocol::DetailsLoadInfoResBody responseBody;
    bool res = parser.GetDetailsLoadInfo(responseBody, false);
    EXPECT_EQ(true, res);
    EXPECT_EQ(8, responseBody.blockIdList.size()); // block id list is 8
    parser.Reset();
}

TEST_F(SourceTest, GetDetailsMemoryGraph) {
    auto &parser = InitParser(g_memoryData);
    Protocol::DetailsMemoryGraphResBody resBody;
    bool res = parser.GetDetailsMemoryGraph("0", false, resBody);
    EXPECT_EQ(true, res);
    parser.Reset();
}

TEST_F(SourceTest, GetDetailsMemoryTable) {
    auto &parser = InitParser(g_memoryData);
    Protocol::DetailsMemoryTableResBody resBody;
    bool res = parser.GetDetailsMemoryTable("", false, resBody);
    EXPECT_EQ(true, res);
    EXPECT_EQ(0, resBody.memoryTable.size());

    res = parser.GetDetailsMemoryTable("32", false, resBody);
    EXPECT_EQ(true, res);
    EXPECT_EQ(0, resBody.memoryTable.size());

    res = parser.GetDetailsMemoryTable("0", false, resBody);
    EXPECT_EQ(true, res);
    EXPECT_EQ(1, resBody.memoryTable.size());
    parser.Reset();
}

TEST_F(SourceTest, GetRoofline) {
    auto &parser = InitParser(g_rooflineDataFile);
    Protocol::DetailsRooflineBody resBody;
    bool res = parser.GetDetailsRoofline(resBody);
    EXPECT_EQ(true, res);
    EXPECT_EQ(resBody.soc, "Ascend910B4");
    int dataSize = 5;
    EXPECT_EQ(resBody.data.size(), dataSize);
    auto item = resBody.data[0];
    EXPECT_EQ(item.title, "Memory Unit(Cube)");
    int rooflineSize = 6;
    EXPECT_EQ(item.rooflines.size(), rooflineSize);
    std::string expect = "18.440187454223633";
    EXPECT_EQ(item.rooflines[0].bw, expect);
    parser.Reset();
}
