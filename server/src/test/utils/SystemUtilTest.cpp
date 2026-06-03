/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include <gtest/gtest.h>
#include "SystemUtil.h"

using namespace Dic;

TEST(SystemUtil, GetCpuCoreCountReturnsPositiveValue) { EXPECT_GT(SystemUtil::GetCpuCoreCount(), 0U); }

TEST(SystemUtil, GetAvailablePhysicalMemoryBytesReturnsPositiveValue) {
    EXPECT_GT(SystemUtil::GetAvailablePhysicalMemoryBytes(), 0U);
}
