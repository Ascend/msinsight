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
#include "GlobalDefs.h"

using namespace Dic;
class GlobalDefsTest : public ::testing::Test {};

TEST_F(GlobalDefsTest, isFileConflict) {
    ProjectTypeEnum typeEnumIPYNB = ProjectTypeEnum::IPYNB;
    ProjectTypeEnum typeEnumDB = ProjectTypeEnum::DB;
    EXPECT_TRUE(isFileConflict(typeEnumIPYNB, typeEnumDB));
    EXPECT_FALSE(isFileConflict(typeEnumDB, typeEnumDB));
}

TEST_F(GlobalDefsTest, PytorchTraceTypeMapping) {
    EXPECT_EQ(static_cast<int>(ParserType::PYTORCH_TRACE_JSON), 11);
    EXPECT_EQ(static_cast<int>(ProjectTypeEnum::PYTORCH_TRACE), 14);
    EXPECT_EQ(coverProjectTypeToParserType(ProjectTypeEnum::PYTORCH_TRACE), ParserType::PYTORCH_TRACE_JSON);
}

TEST_F(GlobalDefsTest, PytorchTraceComparisonCompatibility) {
    EXPECT_TRUE(IsSupportCompareType(ProjectTypeEnum::PYTORCH_TRACE));
    EXPECT_TRUE(IsComparable(ProjectTypeEnum::PYTORCH_TRACE, ProjectTypeEnum::PYTORCH_TRACE));
    EXPECT_FALSE(isFileConflict(ProjectTypeEnum::PYTORCH_TRACE, ProjectTypeEnum::PYTORCH_TRACE));
    EXPECT_TRUE(isFileConflict(ProjectTypeEnum::PYTORCH_TRACE, ProjectTypeEnum::TRACE));
    EXPECT_FALSE(IsComparable(ProjectTypeEnum::PYTORCH_TRACE, ProjectTypeEnum::TRACE));
}
