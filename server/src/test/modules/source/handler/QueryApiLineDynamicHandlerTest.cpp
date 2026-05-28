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
#include "ComputeQuerySourceApiHandlerTest.h"
#include "ComputeQuerySourceApiDynamicHandlerTest.h"
#include "SourceProtocolRequest.h"
#include "QueryCodeFileHandler.h"
#include "QueryApiLineDynamicHandler.h"

using namespace Dic::Server;
using namespace Dic::Module;
using namespace Dic::Module::Source;
using namespace Dic::Module::Source::Test;

class QueryApiLineDynamicHandlerTest {};

class HandlerDerived : public QueryApiLineDynamicHandler {
  public:
    void SetResponseBody(SourceApiLineDynamicResponse &response, SourceApiLineDynamicRequest &request) {
        QueryApiLineDynamicHandler::SetResponseBody(response, request);
    }
};

TEST_F(ComputeQuerySourceApiDynamicHandlerTest, testQueryApiLineDynamicHandlerRequestWithValidData) {
    QueryApiLineDynamicHandler handler;
    auto ptr = std::make_unique<SourceApiLineDynamicRequest>();
    ptr->params.sourceName = SOURCE_NAME;
    ptr->params.coreName = CORE_NAME;
    ptr->moduleName = "Source";
    ptr->projectName = "project";
    handler.HandleRequest(std::move(ptr));
}

TEST_F(ComputeQuerySourceApiDynamicHandlerTest, testQueryApiLineDynamicHandlerSetResponseBodyWithValidData) {
    SourceApiLineDynamicRequest request;
    request.params.sourceName = SOURCE_NAME;
    request.params.coreName = CORE_NAME;
    SourceApiLineDynamicResponse response;
    HandlerDerived handler;
    handler.SetResponseBody(response, request);
    auto columnMap = response.body.columnNameMap;
    EXPECT_EQ(columnMap.size(), 4); // column map size is 4
    EXPECT_EQ(columnMap["Address Range"], 0);
    EXPECT_EQ(columnMap["Cycles"], 1);
    EXPECT_EQ(columnMap["Instructions Executed"], 1);
    EXPECT_EQ(columnMap["Line"], 1);

    auto lines = response.body.sourceFileLines;
    EXPECT_EQ(lines.size(), 8); // lines size is 8
    auto line = lines[1];
    auto addressRange = line.addressRange;
    EXPECT_EQ(addressRange.size(), 2); // address range size is 2
    EXPECT_EQ(addressRange[1].first, "0x1134e0f8");
    EXPECT_EQ(addressRange[1].second, "0x1134e0f8");

    auto map = line.columnValueMap;
    EXPECT_EQ(map.intMap.size(), 3); // int map size is 3
    EXPECT_EQ(map.intMap["Cycles"], 284); // cycles is 284
    EXPECT_EQ(map.intMap["Instructions Executed"], 36); // instructions executed is 36
    EXPECT_EQ(map.intMap["Line"], 32); // line is 32
}

TEST_F(ComputeQuerySourceApiHandlerTest, testQueryApiLineDynamicHandlerRequestWithoutDtype) {
    QueryApiLineDynamicHandler handler;
    auto ptr = std::make_unique<SourceApiLineDynamicRequest>();
    ptr->params.sourceName = SOURCE_NAME;
    ptr->params.coreName = CORE_NAME;
    ptr->moduleName = "Source";
    ptr->projectName = "project";
    handler.HandleRequest(std::move(ptr));
}

TEST_F(ComputeQuerySourceApiHandlerTest, testQueryApiLineDynamicHandlerSetResponseBodyWithoutDtype) {
    SourceApiLineDynamicRequest request;
    request.params.sourceName = SOURCE_NAME;
    request.params.coreName = CORE_NAME;
    SourceApiLineDynamicResponse response;
    HandlerDerived handler;
    handler.SetResponseBody(response, request);
    EXPECT_TRUE(response.body.columnNameMap.empty());

    auto lines = response.body.lines;
    EXPECT_EQ(lines.size(), 8); // lines size is 8
    auto line = lines[1];
    auto addressRange = line.addressRange;
    EXPECT_EQ(addressRange.size(), 2); // address range size is 2
    EXPECT_EQ(addressRange[1].first, "0x1134e0f8");
    EXPECT_EQ(addressRange[1].second, "0x1134e0f8");

    EXPECT_EQ(line.cycle, 284); // cycles is 284
    EXPECT_EQ(line.instructionExecuted, 36); // instructions executed is 36
    EXPECT_EQ(line.line, 32); // line is 32
}
