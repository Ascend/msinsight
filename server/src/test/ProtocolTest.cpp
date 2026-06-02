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

#include "gtest/gtest.h"
#include "ProtocolMessageBuffer.h"

using namespace Dic::Protocol;

class ProtocolTest : public testing::Test {
  public:
    void SetUp() override {}
};

TEST_F(ProtocolTest, MessageWithNoDelim) {
    ProtocolMessageBuffer buffer;
    std::string req = R"(Content-Length: 9)";
    buffer << req;
    auto message = buffer.Pop();
    EXPECT_EQ(message, nullptr);
}

TEST_F(ProtocolTest, MessgeWithWrongLength) {
    ProtocolMessageBuffer buffer;
    std::string req = R"(Content-Length: 1024\r\n\r\nhelloworld)";
    buffer << req;
    auto message = buffer.Pop();
    EXPECT_EQ(message, nullptr);
}

TEST_F(ProtocolTest, MessageCannotCastToJson) {
    ProtocolMessageBuffer buffer;
    std::string req = R"(Content-Length: 10\r\n\r\nhelloworld)";
    buffer << req;
    auto message = buffer.Pop();
    EXPECT_EQ(message, nullptr);
}

TEST_F(ProtocolTest, MessageGetProjectInfo) {
    ProtocolMessageBuffer buffer;
    std::string req = R"({"id":2, "moduleName":"global", "type":"request", "command":"files/getProjectExplorer", )"
                      R"("projectName":"", "params":{}})";
    buffer << R"(Content-Length: 117\r\n\r\n)";
    buffer << req;
    auto message = buffer.Pop();
    EXPECT_NE(message, nullptr);
    EXPECT_EQ(message->moduleName, "global");
}
