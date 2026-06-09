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
#include <vector>

#include "QueryKernelOverallHandler.h"

namespace Dic::Module::Timeline::Test {

TEST(BuildKernelOverallResultTest, EmptyInputYieldsEmptyResult) {
    std::vector<KernelDetailOverallRecord> records;
    KernelOverallResponse response;

    BuildKernelOverallResult(records, response, 1, 10);

    EXPECT_TRUE(response.details.empty());
    EXPECT_EQ(response.pageParam.total, 0U);
}

TEST(BuildKernelOverallResultTest, BuildsAcceleratorCoreColumnAndUniqueKey) {
    std::vector<KernelDetailOverallRecord> records = {
        {"MatMul", "AI_CORE", 2, 30.0, 15.0, 20.0, 10.0},
        {"MatMul", "AI_VECTOR_CORE", 1, 5.0, 5.0, 5.0, 5.0},
    };
    KernelOverallResponse response;

    BuildKernelOverallResult(records, response, 1, 10);

    ASSERT_EQ(response.details.size(), 2U);
    EXPECT_EQ(response.details[0].key, "6:MatMul|7:AI_CORE");
    EXPECT_EQ(response.details[0].type, "MatMul");
    EXPECT_EQ(response.details[0].acceleratorCore, "AI_CORE");
    EXPECT_EQ(response.details[0].number, 2U);
    EXPECT_DOUBLE_EQ(response.details[0].totalTime, 30.0);
    EXPECT_DOUBLE_EQ(response.details[0].avgTime, 15.0);
    EXPECT_DOUBLE_EQ(response.details[0].minTime, 10.0);
    EXPECT_DOUBLE_EQ(response.details[0].maxTime, 20.0);

    EXPECT_EQ(response.details[1].key, "6:MatMul|14:AI_VECTOR_CORE");
    EXPECT_EQ(response.details[1].type, "MatMul");
    EXPECT_EQ(response.details[1].acceleratorCore, "AI_VECTOR_CORE");
}

TEST(BuildKernelOverallResultTest, EmptyTypeAndAcceleratorCoreAreKeptInKey) {
    std::vector<KernelDetailOverallRecord> records = {
        {"", "", 1, 1.0, 1.0, 1.0, 1.0},
    };
    KernelOverallResponse response;

    BuildKernelOverallResult(records, response, 1, 10);

    ASSERT_EQ(response.details.size(), 1U);
    EXPECT_EQ(response.details[0].key, "0:|0:");
    EXPECT_EQ(response.details[0].type, "");
    EXPECT_EQ(response.details[0].acceleratorCore, "");
}

TEST(BuildKernelOverallResultTest, PaginatesAfterResultBuild) {
    std::vector<KernelDetailOverallRecord> records = {
        {"A", "CORE0", 1, 1.0, 1.0, 1.0, 1.0},
        {"B", "CORE1", 1, 2.0, 2.0, 2.0, 2.0},
    };
    KernelOverallResponse response;

    BuildKernelOverallResult(records, response, 2, 1);

    ASSERT_EQ(response.details.size(), 1U);
    EXPECT_EQ(response.pageParam.total, 2U);
    EXPECT_EQ(response.details[0].type, "B");
    EXPECT_EQ(response.details[0].acceleratorCore, "CORE1");
}

} // namespace Dic::Module::Timeline::Test
