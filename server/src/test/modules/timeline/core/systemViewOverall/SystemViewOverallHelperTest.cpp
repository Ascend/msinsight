/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#include <gtest/gtest.h>
#include <utility>
#include "SystemViewOverallHelper.h"

namespace Dic::Module::Timeline {
namespace {
const std::string UTF8_OPERATOR_KEYWORD = "\xE7\xAE\x97\xE5\xAD\x90\xF0\x9F\x98\x80";
CpuCubeOpInfo CreateCpuOp(const std::string &name, uint64_t start, uint64_t end, uint64_t trackId,
    const std::vector<CustomClassificationRule> &rules) {
    CpuCubeOpInfo op{.pythonApi = name, .trackId = trackId, .start = start, .end = end};
    op.CheckCubeOp(rules);
    return op;
}

OverallTmpInfo CreateKernelEvent(
    const std::string &pythonApi, uint64_t flowStartTime, uint64_t flowStartTrackId = 0, double cubeTime = 0) {
    return OverallTmpInfo{.pythonApi = pythonApi,
        .flowStartTime = flowStartTime,
        .flowStartTrackId = flowStartTrackId,
        .cubeTime = cubeTime};
}
}

TEST(SystemViewOverallHelperTest, CustomRulesRespectPriorityAndHierarchy) {
    std::vector<CustomClassificationRule> rules = {
        {"First", {UTF8_OPERATOR_KEYWORD}}, {"Second", {"custom"}}, {"Gate", {"gate"}, false}};
    auto prioritized = CreateKernelEvent(UTF8_OPERATOR_KEYWORD + "_backward", 1, 0, 1);
    auto noDirection = CreateKernelEvent("gate", 1);
    prioritized.GetKernelCategories(rules);
    noDirection.GetKernelCategories(rules);
    EXPECT_EQ(prioritized.categoryList, (std::vector<std::string>{"First", "Backward", "Cube"}));
    EXPECT_EQ(noDirection.categoryList, (std::vector<std::string>{"Gate", "Vector"}));
}
TEST(SystemViewOverallHelperTest, PrioritizesBuiltInAndEarlierCustomRulesWithinFlowStartTrack) {
    std::vector<CustomClassificationRule> rules = {
        {"Custom", {UTF8_OPERATOR_KEYWORD}}, {"Bar", {"bar_wrapper"}}, {"Copy", {"copy"}}, {"Aten", {"aten"}}};
    SystemViewOverallHelper helper;
    helper.cpuCubeOps = {CreateCpuOp(UTF8_OPERATOR_KEYWORD + "_wrapper", 10, 100, 1, rules),
        CreateCpuOp("aten::matmul", 20, 40, 1, rules), CreateCpuOp("bar_wrapper", 20, 80, 2, rules),
        CreateCpuOp("aten::wrapper", 110, 200, 1, rules), CreateCpuOp("copy_wrapper", 120, 180, 1, rules)};
    helper.kernelEvents = {CreateKernelEvent("", 30, 1, 1), CreateKernelEvent("", 30, 2, 1),
        CreateKernelEvent("", 70, 1, 1), CreateKernelEvent("", 130, 1, 1)};
    helper.CategorizeComputingEvents(rules);
    ASSERT_EQ(helper.kernelEvents.size(), 4);
    EXPECT_EQ(helper.kernelEvents[0].categoryList, (std::vector<std::string>{"Matmul", "Cube"}));
    EXPECT_EQ(helper.kernelEvents[1].categoryList, (std::vector<std::string>{"Bar", "Forward", "Cube"}));
    EXPECT_EQ(helper.kernelEvents[2].categoryList, (std::vector<std::string>{"Custom", "Forward", "Cube"}));
    EXPECT_EQ(helper.kernelEvents[3].categoryList, (std::vector<std::string>{"Copy", "Forward", "Cube"}));
}
TEST(SystemViewOverallHelperTest, CustomDirectionMarkersRequireBoundaries) {
    std::vector<CustomClassificationRule> rules = {{"Custom", {"callback", "backbone", "backend", "rms_norm"}}};
    const std::vector<std::pair<std::string, std::string>> cases = {{"aten::callback_forward", "Forward"},
        {"model_backbone_block", "Forward"}, {"backend_rmsnorm", "Forward"}, {"rms_norm_backward", "Backward"},
        {"rms_norm_bwd", "Backward"}, {"rms_norm_grad", "Backward"}};
    for (const auto &[pythonApi, direction] : cases) {
        SCOPED_TRACE(pythonApi);
        auto event = CreateKernelEvent(pythonApi, 1);
        event.GetKernelCategories(rules);
        EXPECT_EQ(event.categoryList, (std::vector<std::string>{"Custom", direction, "Vector"}));
    }
}
TEST(SystemViewOverallHelperTest, ReportsUnmatchedKeywordsOnlyWithComputingData) {
    SystemViewOverallHelper helper;
    helper.kernelEvents = {CreateKernelEvent(UTF8_OPERATOR_KEYWORD + "_api", 0)};
    std::vector<CustomClassificationRule> rules = {{"Custom", {UTF8_OPERATOR_KEYWORD, "missing_keyword"}}};
    auto unmatchedKeywords = helper.GetUnmatchedCustomClassificationKeywords(rules);
    EXPECT_EQ(unmatchedKeywords, (std::vector<std::string>{"missing_keyword"}));
    helper.kernelEvents.clear();
    EXPECT_TRUE(helper.GetUnmatchedCustomClassificationKeywords(rules).empty());
}
TEST(SystemViewOverallHelperTest, CustomGroupingDoesNotCreateRoundingOtherCategory) {
    SystemViewOverallHelper helper;
    helper.e2eTime = 1;
    helper.kernelEvents = {OverallTmpInfo{.duration = 0.1, .categoryList = {"First", "Vector"}},
        OverallTmpInfo{.duration = 0.2, .categoryList = {"Second", "Vector"}}};
    std::vector<SystemViewOverallRes> response = {
        SystemViewOverallRes{.totalTime = 0.3, .name = OVERALL_CAT_COMPUTING}};
    helper.AggregateComputingOverallMetrics(response);
    ASSERT_EQ(response[0].children.size(), 2);
    EXPECT_EQ(response[0].children[0].name, "First");
    EXPECT_EQ(response[0].children[1].name, "Second");
}
}
