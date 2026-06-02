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
#include "SafeQueue.h"

using namespace Dic;

class SafeQueueTest : public testing::Test {};

TEST_F(SafeQueueTest, testPop) {
    SafeQueue<int> q;
    int res = 1; // push 1
    q.Push(res);
    EXPECT_EQ(q.Empty(), false);
    EXPECT_EQ(q.Size(), 1); // size 1
    int res2 = 0;
    q.Pop(res2);
    EXPECT_EQ(res, 1);
    q.Push(res);
    q.Clear();
    EXPECT_EQ(q.Empty(), true);
}
