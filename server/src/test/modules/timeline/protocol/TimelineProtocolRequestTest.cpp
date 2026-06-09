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
#include "PythonStackHelper.h"
#include "TimelineProtocolRequest.h"
class TimelineProtocolRequestTest : public ::testing::Test {};

TEST_F(TimelineProtocolRequestTest, TestImportActionParams) {
    Dic::Protocol::ImportActionParams params;
    params.projectName = "ll";
    params.projectAction = Dic::Protocol::ProjectActionEnum::UNKNOWN;
    std::string errorMsg;
    bool res = params.CommonCheck(errorMsg);
    EXPECT_EQ(res, false);
    params.projectAction = Dic::Protocol::ProjectActionEnum::TRANSFER_PROJECT;
    res = params.CommonCheck(errorMsg);
    EXPECT_EQ(res, true);
    res = params.ConvertToRealPath(errorMsg);
    EXPECT_EQ(res, false);
    params.path.emplace_back("LLLLLLLLLL");
    res = params.ConvertToRealPath(errorMsg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, TestUnitThreadTracesParams) {
    Dic::Protocol::UnitThreadTracesParams params;
    const uint64_t st = 9;
    const uint64_t en = 2;
    params.startTime = st;
    params.endTime = en;
    std::string errorMsg;
    const uint64_t min = 7;
    bool res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    params.endTime = UINT64_MAX;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    const uint64_t mi = 89;
    params.endTime = mi;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, true);
}

TEST_F(TimelineProtocolRequestTest, UnitThreadTracesSummaryParams) {
    Dic::Protocol::UnitThreadTracesSummaryParams params;
    const uint64_t st = 9;
    const uint64_t en = 2;
    params.startTime = st;
    params.endTime = en;
    std::string errorMsg;
    const uint64_t min = 7;
    bool res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    params.endTime = UINT64_MAX;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    const uint64_t mi = 89;
    params.endTime = mi;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, true);
}

TEST_F(TimelineProtocolRequestTest, UnitThreadsParams) {
    Dic::Protocol::UnitThreadsParams params;
    const uint64_t st = 9;
    const uint64_t en = 2;
    params.startTime = st;
    params.endTime = en;
    std::string errorMsg;
    const uint64_t min = 7;
    bool res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    params.endTime = UINT64_MAX;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    const uint64_t mi = 89;
    std::string startDepth = "";
    std::string endDepth = "";
    params.endTime = mi;
    params.startDepth = startDepth;
    params.endDepth = endDepth;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, true);
}

TEST_F(TimelineProtocolRequestTest, UnitFlowsParams) {
    Dic::Protocol::UnitFlowsParams params;
    const uint64_t st = 9;
    const uint64_t en = 2;
    params.startTime = st;
    params.endTime = en;
    std::string errorMsg;
    const uint64_t min = 7;
    bool res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    params.endTime = UINT64_MAX;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    const uint64_t mi = 89;
    params.endTime = mi;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, true);
}

TEST_F(TimelineProtocolRequestTest, FlowCategoryEventsParams) {
    Dic::Protocol::FlowCategoryEventsParams params;
    const uint64_t st = 9;
    const uint64_t en = 2;
    params.startTime = st;
    params.endTime = en;
    std::string errorMsg;
    const uint64_t min = 7;
    bool res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    params.endTime = UINT64_MAX;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    const uint64_t mi = 89;
    params.endTime = mi;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, true);
}

TEST_F(TimelineProtocolRequestTest, TestUnitCounterParams) {
    Dic::Protocol::UnitCounterParams params;
    const uint64_t st = 9;
    const uint64_t en = 2;
    params.startTime = st;
    params.endTime = en;
    std::string errorMsg;
    const uint64_t min = 7;
    bool res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    params.endTime = UINT64_MAX;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    const uint64_t mi = 89;
    params.endTime = mi;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, true);
}

TEST_F(TimelineProtocolRequestTest, EventsViewParams) {
    Dic::Protocol::EventsViewParams params;
    params.pid = "test";
    params.pageSize = 0;
    std::string msg;
    bool res = params.CheckParams(0, msg);
    EXPECT_EQ(res, false);
    params.pageSize = 1;
    params.currentPage = 1;
    params.filters.emplace_back("--", "");
    res = params.CheckParams(0, msg);
    EXPECT_EQ(res, false);
    params.filters.clear();
    params.startTime = 100;
    params.endTime = 1;
    res = params.CheckParams(0, msg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, SystemViewOverallReqParams) {
    Dic::Protocol::SystemViewOverallReqParam params;
    params.page.pageSize = 0;
    std::string msg;
    bool res = params.CheckParams(0, msg);
    EXPECT_EQ(res, false);
    params.page.pageSize = 1;
    params.page.current = 0;
    res = params.CheckParams(0, msg);
    EXPECT_EQ(res, false);
    params.page.current = 1;
    params.startTime = 100;
    params.endTime = 1;
    res = params.CheckParams(0, msg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, TestUnitThreadsOperatorsParams) {
    Dic::Protocol::UnitThreadsOperatorsParams params;
    const uint64_t st = 9;
    const uint64_t en = 2;
    params.startTime = st;
    params.endTime = en;
    std::string errorMsg;
    const uint64_t min = 7;
    bool res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    params.endTime = UINT64_MAX;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
    const uint64_t mi = 89;
    params.endTime = mi;
    res = params.CheckParams(min, errorMsg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, KernelOverallParams) {
    Dic::Protocol::KernelOverallRequest::Params params;
    std::string errorMsg;

    params.page.pageSize = 0;
    params.page.current = 1;
    EXPECT_EQ(params.CheckParams(0, errorMsg), false);

    params.page.pageSize = 1;
    params.page.current = 0;
    EXPECT_EQ(params.CheckParams(0, errorMsg), false);

    params.page.current = 1;
    params.startTime = 2;
    params.endTime = 1;
    EXPECT_EQ(params.CheckParams(0, errorMsg), false);

    params.startTime = 0;
    params.endTime = UINT64_MAX;
    EXPECT_EQ(params.CheckParams(1, errorMsg), false);

    params.endTime = 1;
    params.order = "";
    params.orderBy = "invalid";
    EXPECT_EQ(params.CheckParams(0, errorMsg), true);

    params.order = "ascend";
    params.orderBy = "acceleratorCore";
    EXPECT_EQ(params.CheckParams(0, errorMsg), true);

    params.orderBy = "invalid";
    EXPECT_EQ(params.CheckParams(0, errorMsg), false);

    params.order = "invalid";
    params.orderBy = "type";
    EXPECT_EQ(params.CheckParams(0, errorMsg), false);
}

TEST_F(TimelineProtocolRequestTest, TestRankOffsetParamsValidInput) {
    Dic::Protocol::RankOffsetParams params;
    params.sliceName = "MatMul";
    params.rankId = "0";
    params.fileId = "file_0";
    params.pid = "1234";
    params.metaType = "Ascend Hardware";
    params.alignType = "LEFT";
    std::string errorMsg;
    bool res = params.CheckParams(errorMsg);
    EXPECT_EQ(res, true);
}

TEST_F(TimelineProtocolRequestTest, TestRankOffsetParamsMissingSliceName) {
    Dic::Protocol::RankOffsetParams params;
    params.rankId = "0";
    params.fileId = "file_0";
    params.pid = "1234";
    params.metaType = "Ascend Hardware";
    params.alignType = "LEFT";
    std::string errorMsg;
    bool res = params.CheckParams(errorMsg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, TestRankOffsetParamsMissingRankId) {
    Dic::Protocol::RankOffsetParams params;
    params.sliceName = "MatMul";
    params.fileId = "file_0";
    params.pid = "1234";
    params.metaType = "Ascend Hardware";
    params.alignType = "LEFT";
    std::string errorMsg;
    bool res = params.CheckParams(errorMsg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, TestRankOffsetParamsMissingFileId) {
    Dic::Protocol::RankOffsetParams params;
    params.sliceName = "MatMul";
    params.rankId = "0";
    params.pid = "1234";
    params.metaType = "Ascend Hardware";
    params.alignType = "LEFT";
    std::string errorMsg;
    bool res = params.CheckParams(errorMsg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, TestRankOffsetParamsMissingPid) {
    Dic::Protocol::RankOffsetParams params;
    params.sliceName = "MatMul";
    params.rankId = "0";
    params.fileId = "file_0";
    params.metaType = "Ascend Hardware";
    params.alignType = "LEFT";
    std::string errorMsg;
    bool res = params.CheckParams(errorMsg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, TestRankOffsetParamsMissingMetaType) {
    Dic::Protocol::RankOffsetParams params;
    params.sliceName = "MatMul";
    params.rankId = "0";
    params.fileId = "file_0";
    params.pid = "1234";
    params.alignType = "LEFT";
    std::string errorMsg;
    bool res = params.CheckParams(errorMsg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, TestRankOffsetParamsMissingAlignType) {
    Dic::Protocol::RankOffsetParams params;
    params.sliceName = "MatMul";
    params.rankId = "0";
    params.fileId = "file_0";
    params.pid = "1234";
    params.metaType = "Ascend Hardware";
    std::string errorMsg;
    bool res = params.CheckParams(errorMsg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, TestRankOffsetParamsInvalidAlignType) {
    Dic::Protocol::RankOffsetParams params;
    params.sliceName = "MatMul";
    params.rankId = "0";
    params.fileId = "file_0";
    params.pid = "1234";
    params.metaType = "Ascend Hardware";
    params.alignType = "TOP";
    std::string errorMsg;
    bool res = params.CheckParams(errorMsg);
    EXPECT_EQ(res, false);
}

TEST_F(TimelineProtocolRequestTest, TestRankOffsetParamsRightAlign) {
    Dic::Protocol::RankOffsetParams params;
    params.sliceName = "MatMul";
    params.rankId = "0";
    params.fileId = "file_0";
    params.pid = "1234";
    params.metaType = "PYTORCH_API";
    params.alignType = "RIGHT";
    std::string errorMsg;
    bool res = params.CheckParams(errorMsg);
    EXPECT_EQ(res, true);
}

TEST_F(TimelineProtocolRequestTest, RestoreTextPythonStackThreadTracesParamsWhenPidEqualsTid) {
    Dic::Protocol::UnitThreadTracesParams params;
    params.processId = "100";
    params.threadId = "python_stack:text:100";
    params.metaType = "PYTORCH_API_PYTHON_STACK";

    bool restored = Dic::Module::Timeline::PythonStackHelper::RestoreThreadTracesParams(params);

    EXPECT_EQ(restored, true);
    EXPECT_EQ(params.isPythonStack, true);
    EXPECT_EQ(params.threadId, "100");
    EXPECT_EQ(params.metaType, "TEXT");
}

TEST_F(TimelineProtocolRequestTest, RestoreFullDbPythonStackThreadTracesParams) {
    Dic::Protocol::UnitThreadTracesParams params;
    params.processId = "4294967297";
    params.threadId = "python_stack:4294967297";
    params.metaType = "PYTORCH_API_PYTHON_STACK";

    bool restored = Dic::Module::Timeline::PythonStackHelper::RestoreThreadTracesParams(params);

    EXPECT_EQ(restored, true);
    EXPECT_EQ(params.isPythonStack, true);
    EXPECT_EQ(params.threadId, "pytorch");
    EXPECT_EQ(params.metaType, "PYTORCH_API");
}
