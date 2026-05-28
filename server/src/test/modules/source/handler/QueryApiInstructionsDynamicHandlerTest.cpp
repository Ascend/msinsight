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
#include "QueryApiInstructionsDynamicHandler.h"
#include "ComputeSourceFile.h"
#include "SourceProtocolRequest.h"
#include "ServerDefs.h"

using namespace Dic::Server;
using namespace Dic::Module;
using namespace Dic::Module::Source;
using namespace Dic::Module::Source::Test;

class QueryApiInstructionsDynamicHandlerTest {};

class HandlerDerived : public QueryApiInstructionsDynamicHandler {
  public:
    void SetResponseBody(SourceApiInstrDynamicResponse &response, SourceApiInstrDynamicRequest &request) {
        QueryApiInstructionsDynamicHandler::SetResponseBody(response, request);
    }
};

TEST_F(ComputeQuerySourceApiDynamicHandlerTest, testQueryApiInstructionsDynamicHandlerRequestWithValidData) {
    QueryApiInstructionsDynamicHandler handler;
    auto ptr = std::make_unique<SourceApiInstrDynamicRequest>();
    ptr->params.coreName = CORE_NAME;
    ptr->moduleName = "Source";
    ptr->projectName = "project";
    handler.HandleRequest(std::move(ptr));
}

TEST_F(ComputeQuerySourceApiDynamicHandlerTest, testQueryApiInstructionsDynamicHandlerSetResponseBodyWithValidData) {
    HandlerDerived handler;
    SourceApiInstrDynamicRequest request;
    request.params.coreName = CORE_NAME;
    SourceApiInstrDynamicResponse response;
    handler.SetResponseBody(response, request);
    EXPECT_EQ(response.body.coreName, CORE_NAME);
    auto colMap = response.body.columnNameMap;
    EXPECT_EQ(colMap["Address"], ColumnDataType::Type::STRING);
    EXPECT_EQ(colMap["AscendC Inner Code"], ColumnDataType::Type::STRING);
    EXPECT_EQ(colMap["Cycles"], ColumnDataType::Type::INT);
    EXPECT_EQ(colMap["Instructions Executed"], ColumnDataType::Type::INT);
    EXPECT_EQ(colMap["Pipe"], ColumnDataType::Type::STRING);
    EXPECT_EQ(colMap["TheoreticalStallCycles"], ColumnDataType::Type::INT);
    EXPECT_EQ(colMap["Source"], ColumnDataType::Type::STRING);
    EXPECT_EQ(colMap["RealStallCycles"], ColumnDataType::Type::INT);

    auto dataList = response.body.columnValues;
    EXPECT_TRUE(!dataList.empty());
    auto data = dataList[0];
    EXPECT_EQ(data.stringMap["Address"], "0x1134e288");
    EXPECT_EQ(data.stringMap["AscendC Inner Code"],
        "/test/compiler/tikcpp/tikcfw/interface/kernel_operator_simt_float_intrinsics.h:104");
    EXPECT_EQ(data.intMap["Cycles"], 32); // Cycles is 62
    EXPECT_EQ(data.intMap["Instructions Executed"], 4); // Instructions Executed is 4
    EXPECT_EQ(data.stringMap["Pipe"], "RVECEX");
    EXPECT_EQ(data.intMap["TheoreticalStallCycles"], 8); // TheoreticalStallCycles is 8
    EXPECT_EQ(data.stringMap["Source"],
        "SIMT_IADD [PEX:6|P],[Rm:3|R],[Rn:8|R],[Rd:3|R],[waitBitMask:3],[stallCyc:7],[yeild:0],[inv:0]");
    EXPECT_EQ(data.intMap["RealStallCycles"], 75); // RealStallCycles is 13
}

TEST_F(ComputeQuerySourceApiHandlerTest, testQueryApiInstructionsDynamicHandlerRequestWithValidData) {
    QueryApiInstructionsDynamicHandler handler;
    auto ptr = std::make_unique<SourceApiInstrDynamicRequest>();
    ptr->params.coreName = CORE_NAME;
    ptr->moduleName = "Source";
    ptr->projectName = "project";
    handler.HandleRequest(std::move(ptr));
}

TEST_F(ComputeQuerySourceApiHandlerTest, testQueryApiInstructionsDynamicHandlerSetResponseBodyWithoutDtype) {
    HandlerDerived handler;
    SourceApiInstrDynamicRequest request;
    request.params.coreName = CORE_NAME;
    SourceApiInstrDynamicResponse response;
    handler.SetResponseBody(response, request);
    EXPECT_EQ(response.body.coreName, CORE_NAME);
    EXPECT_TRUE(response.body.columnNameMap.empty());

    auto dataList = response.body.instructions;
    EXPECT_TRUE(!dataList.empty());
    auto data = dataList[0];
    EXPECT_EQ(data.address, "0x1134e2d8");
    EXPECT_EQ(data.ascendCInnerCode, "/test/vec_add1_simt.cpp:50");
    EXPECT_EQ(data.cycles, 62); // Cycles is 62
    EXPECT_EQ(data.instructionsExecuted, 4); // Instructions Executed is 4
    EXPECT_EQ(data.pipe, "RVECLD");
    EXPECT_EQ(data.theoreticalStallCycles, 8); // TheoreticalStallCycles is 8
    EXPECT_EQ(data.source, "SIMT_LDG [PEX:6|P],[Rn:0|R],[Rn1:1|R],[Rd:0|R],[#ofst:9],[cop:1],[l2_cache_hint:0]");
    EXPECT_EQ(data.realStallCycles, 13); // RealStallCycles is 13
}
