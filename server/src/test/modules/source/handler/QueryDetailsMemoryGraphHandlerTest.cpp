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
#include "../mockUtils/BinFileGenerator.h"
#include "WsSessionManager.h"
#include "ServerDefs.h"
#include "WsSessionImpl.h"
#include "../DetailsMemoryJson.h"
#include "QueryDetailsMemoryGraphHandler.h"
#include "QueryDetailsMemoryTableHandler.h"
#include "SourceProtocolRequest.h"

using namespace Dic::Server;
using namespace Dic::Module;
using namespace Dic::Module::Source;
using namespace Dic::Module::Source::Test;

static const std::string BIN_FILE_PATH = "query_details_memory_graph_test.bin";
class QueryDetailsMemoryGraphHandlerTest {};
class ComputeQueryDetailsMemoryGraphHandlerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        // generate bin file
        auto graph = std::make_unique<NormalDataBlock>(DataTypeEnum::DETAILS_MEMORY_GRAPH, DETAILS_MEMORY_GRAPH_JSON);
        auto table = std::make_unique<NormalDataBlock>(DataTypeEnum::DETAILS_MEMORY_TABLE, DETAILS_MEMORY_TABLE_JSON);
        BinFileGenerator generator;

        generator.AddDataBlock(std::move(graph));
        generator.AddDataBlock(std::move(table));
        generator.Generate(BIN_FILE_PATH);

        // init parser
        SourceFileParser::Instance().SetFilePath(BIN_FILE_PATH);
        SourceFileParser::Instance().Parse({BIN_FILE_PATH}, BIN_FILE_PATH, BIN_FILE_PATH, "");
    }

    static void TearDownTestSuite() {
        // remove bin file
        Dic::Module::Source::Test::BinFileGenerator::RemoveFile(BIN_FILE_PATH);

        // reset parser
        SourceFileParser::Instance().Reset();
    }
};

TEST_F(ComputeQueryDetailsMemoryGraphHandlerTest, testQueryDetailsMemoryGraphHandlerWithValidData) {
    QueryDetailsMemoryGraphHandler handler;
    auto ptr = std::make_unique<DetailsMemoryGraphRequest>();
    ptr->params.blockId = "0";
    handler.HandleRequest(std::move(ptr));
}

TEST_F(ComputeQueryDetailsMemoryGraphHandlerTest, testQueryDetailsMemoryTableHandlerWithValidData) {
    QueryDetailsMemoryTableHandler handler;
    auto ptr = std::make_unique<DetailsMemoryTableRequest>();
    ptr->params.blockId = "0";
    handler.HandleRequest(std::move(ptr));
}
