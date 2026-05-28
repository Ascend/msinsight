/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include <memory>
#include "QueryTritonMemoryBlocksHandler.h"
#include "QueryTritonBasicInfoHandler.h"
#include "QueryTritonMemoryUsageHandler.h"
#include "TritonProtocolRequest.h"
#include "TritonProtocolResponse.h"
#include "TritonService.h"

using namespace Dic::Module::Triton;
using namespace Dic::Protocol;

class TritonHandlerTest : public ::testing::Test {
  protected:
    void SetUp() override { TritonService::Instance().Reset(); }

    void TearDown() override { TritonService::Instance().Reset(); }
};

/**
 * @brief 场景说明：测试 QueryTritonMemoryBlocksHandler 处理请求的功能。
 * 验证 Handler 能正确从 Service 获取数据并填充到 Response 中。
 */
TEST_F(TritonHandlerTest, QueryTritonMemoryBlocksHandlerTest) {
    // 准备数据
    TritonTensorSegment s;
    s.start = 100;
    s.end = 200;
    TritonTensorBlock b;
    b.id = "1";
    s.blocks.push_back(b);
    std::map<std::string, TritonRecord> records;
    TritonRecord record;
    record.segments.push_back(s);
    records["scope_test"] = record;
    TritonService::Instance().UpdateRecord(std::move(records));

    // 构造请求
    auto req = std::make_unique<TritonMemoryBlocksRequest>();
    req->startTimestamp = 100;
    req->endTimestamp = 200;
    req->scopeType = "scope_test";

    QueryTritonMemoryBlocksHandler handler;
    // 注意：HandleRequest 内部会调用 SendResponse。
    // 在单元测试中，我们可能需要 mock WsSender 或检查 Response 对象的副作用。
    // 由于 HandleRequest 返回 bool，我们先验证其基本逻辑。
    bool result = handler.HandleRequest(std::move(req));
    EXPECT_TRUE(result);
}

/**
 * @brief 场景说明：测试 QueryTritonBasicInfoHandler 处理请求的功能。
 */
TEST_F(TritonHandlerTest, QueryTritonBasicInfoHandlerTest) {
    TritonMemeHeader header;
    header.kernelName = "test_kernel";
    header.memTypes = {"scope_test"};
    TritonService::Instance().SetHeader(std::move(header));

    auto req = std::make_unique<TritonBasicInfoRequest>();
    QueryTritonBasicInfoHandler handler;

    bool result = handler.HandleRequest(std::move(req));
    EXPECT_TRUE(result);
}

/**
 * @brief 场景说明：测试 QueryTritonMemoryUsageHandler 处理请求的功能。
 */
TEST_F(TritonHandlerTest, QueryTritonMemoryUsageHandlerTest) {
    TritonTensorSegment s;
    s.start = 100;
    s.end = 200;
    std::map<std::string, TritonRecord> records;
    TritonRecord record;
    record.segments.push_back(s);
    records["scope_test"] = record;
    TritonService::Instance().UpdateRecord(std::move(records));

    auto req = std::make_unique<TritonMemoryUsageRequest>();
    req->timestamp = 150;
    req->scopeType = "scope_test";

    QueryTritonMemoryUsageHandler handler;
    bool result = handler.HandleRequest(std::move(req));
    EXPECT_TRUE(result);
}
