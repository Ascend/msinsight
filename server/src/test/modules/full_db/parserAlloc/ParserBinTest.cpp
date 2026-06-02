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
#include "ProjectParserBin.h"
#include "TrackInfoManager.h"
#include "../../source/mockUtils/BinFileGenerator.h"
#include "../../source/DetailsComputeLoadJson.h"

using namespace Dic::Module;
using namespace Dic::Module::Global;
using namespace Dic::Module::Source::Test;

class ParserBinTest : public ::testing::Test {};

TEST_F(ParserBinTest, ParserWithValidData) {
    std::string fileName = "ParserBinTest.bin";
    std::string filePath = "./" + fileName;

    // generate bin file
    NormalDataBlock baseInfoDataBlock(DataTypeEnum::DETAILS_BASE_INFO, DETAILS_BASE_INFO_JSON);
    BinFileGenerator generator;
    generator.AddDataBlock(std::make_unique<NormalDataBlock>(baseInfoDataBlock));
    generator.Generate(fileName);

    std::vector<std::shared_ptr<ParseFileInfo>> filePathInfos;
    auto fileInfo = std::make_shared<ParseFileInfo>();
    fileInfo->id = 1;
    fileInfo->projectExplorerId = 1;
    fileInfo->clusterId = filePath;
    fileInfo->parseFilePath = "";
    filePathInfos.emplace_back(fileInfo);
    ProjectExplorerInfo info = {1, "testProject", filePath, filePathInfos, {}, {}, "", {""}, ""};
    ImportActionRequest request;
    request.params = {"testProject", {filePath}, ProjectActionEnum::ADD_FILE, false};
    ImportActionResponse response;
    ProjectParserBin parserBin;
    parserBin.Parser({info}, request, response);
    BaselineInfo baselineInfo = {"localhost", "0", "card0", "", false};
    parserBin.ParserBaseline({info}, baselineInfo);

    // remove file
    BinFileGenerator::RemoveFile(fileName);
    SourceFileParser::Instance().Reset();
}

TEST_F(ParserBinTest, ResponseHasRankList) {
    std::string fileName = "ParserBinTest.bin";
    std::string filePath = "./" + fileName;

    // generate bin file
    NormalDataBlock baseInfoDataBlock(DataTypeEnum::DETAILS_BASE_INFO, DETAILS_BASE_INFO_JSON);
    BinFileGenerator generator;
    generator.AddDataBlock(std::make_unique<NormalDataBlock>(baseInfoDataBlock));
    generator.Generate(fileName);

    std::vector<std::shared_ptr<ParseFileInfo>> filePathInfos;
    auto fileInfo = std::make_shared<ParseFileInfo>();
    fileInfo->id = 1;
    fileInfo->projectExplorerId = 1;
    fileInfo->clusterId = filePath;
    fileInfo->parseFilePath = "";
    filePathInfos.emplace_back(fileInfo);
    ProjectExplorerInfo info = {1, "testProject", filePath, filePathInfos, {}, {}, "", {""}, ""};
    ImportActionRequest request;
    request.params = {"testProject", {filePath}, ProjectActionEnum::ADD_FILE, false};

    ProjectParserBin parserBin;
    ImportActionResponse response;
    parserBin.Parser({info}, request, response);
    auto rankList = Timeline::TrackInfoManager::Instance().GetRankListByFileId(
        "./ParserBinTest_mindstudio_insight_data.db", "./ParserBinTest.bin");
    EXPECT_FALSE(rankList.empty());
    // remove file
    BinFileGenerator::RemoveFile(fileName);
    SourceFileParser::Instance().Reset();
}

TEST_F(ParserBinTest, get_import_file) {
    ProjectParserBin bin;
    std::string error;
    auto res = bin.GetParseFileByImportFile("test", error);
    EXPECT_EQ(res.size(), 1);
}
