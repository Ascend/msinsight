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
#include "MemoryErrorManager.h"

using namespace Dic::Module::Memory;

TEST(MemoryErrorManagerTest, EveryDeclaredErrorHasAConcreteMessage) {
    for (const auto &[code, message] : errorMessages) {
        EXPECT_EQ(GetErrorMessage(code), message);
        EXPECT_FALSE(message.empty());
        EXPECT_NE(message, "Unknown error code");
    }
}

TEST(MemoryErrorManagerTest, UnknownErrorReturnsFallbackMessage) {
    EXPECT_EQ(GetErrorMessage(static_cast<ErrorCode>(-1)), "Unknown error code");
}
