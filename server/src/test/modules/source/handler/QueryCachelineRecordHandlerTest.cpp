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
#include "SourceProtocolRequest.h"
#include "QueryCachelineRecordHandler.h"

using namespace Dic::Server;
using namespace Dic::Module;
using namespace Dic::Module::Source;
using namespace Dic::Module::Source::Test;
const std::string DETAILS_CACHE_JSON = R"JSON({
    "Cacheline Records" : [
        {
            "Cacheline Id" : 138,
            "Hit" : {
                "Address Range" : [],
                "Value" : [0, 0.0]
            },
            "Miss" : {
                "Address Range" : [],
                "Value" : [0, 0.0]
            }
        },
        {
            "Cacheline Id" : 138,
            "Hit" : {
                "Address Range" : [],
                "Value" : [0, 0.0]
            },
            "Miss" : {
                "Address Range" : [],
                "Value" : [0, 0.0]
            }
        }
    ]
})JSON";

static const std::string BIN_FILE_PATH = "visualize_data_nosource.bin";

class QueryCachelineRecordHandlerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        auto cache = std::make_unique<NormalDataBlock>(DataTypeEnum::DISPLAY_CACHE, DETAILS_CACHE_JSON);
        BinFileGenerator generator;
        generator.AddDataBlock(std::move(cache));
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

TEST_F(QueryCachelineRecordHandlerTest, QueryCachelineRecordHandlerWithValidData) {
    QueryCachelineRecordHandler handler;
    auto ptr = std::make_unique<DetailsMemoryGraphRequest>();
    EXPECT_TRUE(handler.HandleRequest(std::move(ptr)));
}
