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

#include "DetailsMemoryParserTest.h"
#include "DetailsMemoryParser.h"
#include "SourceProtocol.h"
#include "mockUtils/BinFileGenerator.h"

using namespace Dic::Module::Source;
using namespace Dic::Protocol;
using namespace Dic::Module::Source::Test;

TEST_F(DetailsMemoryParserTest, testGetDetailsBaseInfoWithValidData) {
    // 构建bin文件
    std::string binFilePath = "validDetailsBaseInfo.bin";
    NormalDataBlock dataBlock(DataTypeEnum::DETAILS_BASE_INFO, VALID_DETAILS_BASE_INFO_JSON);
    BinFileGenerator generator;
    generator.AddDataBlock(std::make_unique<NormalDataBlock>(dataBlock));
    generator.Generate(binFilePath);

    DetailsMemoryParser parser;
    DetailsBaseInfoResBody body;
    std::map<int, std::vector<Position>> map;
    int start = 12;
    int end = 665;
    map[static_cast<int>(DataTypeEnum::DETAILS_BASE_INFO)].push_back({start, end});
    parser.GetDetailsBaseInfo(body, binFilePath, map);
    EXPECT_EQ(body.soc, "Ascend910B4");
    EXPECT_EQ(body.duration, "205.5041046142578");
    EXPECT_EQ(body.opType, "mix");
    EXPECT_EQ(body.deviceId, "0");
    EXPECT_EQ(body.name, "MatmulCustom_1396c02f7d7795e4c5cf7dddc5abe6da_1_mix_aic");
    EXPECT_EQ(body.pid, "4117480");
    EXPECT_EQ(body.blockDim, "1");
    EXPECT_EQ(body.mixBlockDim, "2");
    int headNameSize = 4;
    EXPECT_EQ(body.blockDetail.headerName.size(), headNameSize);
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;
    int index3 = 3;
    EXPECT_EQ(body.blockDetail.headerName[index0], "Block ID");
    EXPECT_EQ(body.blockDetail.headerName[index1], "Cube0 Duration (μs)");
    EXPECT_EQ(body.blockDetail.headerName[index2], "Vector0 Duration (μs)");
    EXPECT_EQ(body.blockDetail.headerName[index3], "Vector1 Duration (μs)");
    int rowSize = 1;
    EXPECT_EQ(body.blockDetail.row.size(), rowSize);
    int sizeSize = 2;
    EXPECT_EQ(body.blockDetail.size.size(), sizeSize);
    EXPECT_EQ(body.blockDetail.size[index0], "2");
    EXPECT_EQ(body.blockDetail.size[index1], "4");

    // 删除bin文件
    generator.RemoveFile(binFilePath);
}
