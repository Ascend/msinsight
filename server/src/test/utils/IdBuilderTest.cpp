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
#include "IdBuilder.h"

using namespace Dic;

TEST(IdBuilderTest, TestIdBuilder) {
    int id2 = IdBuilder::EventIdBuilder().Build();
    int id3 = IdBuilder::RequestIdBuilder().Build();
    int id4 = IdBuilder::SessionIdBuilder().Build();
    EXPECT_EQ(id2, 0);
    EXPECT_EQ(id3, 0);
    EXPECT_EQ(id4, 0);
}
