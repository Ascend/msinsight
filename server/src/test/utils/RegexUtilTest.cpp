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
#include "../TestSuit.h"
#include "RegexUtil.h"

using namespace Dic;

TEST(RegexUtil, RegexSearch) {
    std::string data = "AaaAaaaA";
    std::string pattern;
    std::optional<std::smatch> smatch;

    pattern = "a+";
    smatch = RegexUtil::RegexSearch(data, pattern);
    EXPECT_EQ(smatch->str(), "aa");

    pattern = "aaAa+";
    smatch = RegexUtil::RegexSearch(data, pattern);
    EXPECT_EQ(smatch->str(), "aaAaaa");

    pattern = "AaaAaaaA";
    smatch = RegexUtil::RegexSearch(data, pattern);
    EXPECT_EQ(smatch->str(), "AaaAaaaA");

    pattern = "ww";
    smatch = RegexUtil::RegexSearch(data, pattern);
    EXPECT_EQ(smatch.has_value(), false);
}

TEST(RegexUtil, RegexMatch) {
    std::string data = "AaaAaaaA";
    std::string pattern;
    std::optional<std::smatch> smatch;

    pattern = "AaaAaaaA";
    smatch = RegexUtil::RegexMatch(data, pattern);
    EXPECT_EQ(smatch.value()[0], "AaaAaaaA");

    pattern = "Aa+Aa+aA";
    smatch = RegexUtil::RegexMatch(data, pattern);
    EXPECT_EQ(smatch.value()[0], "AaaAaaaA");

    pattern = "\\w+";
    smatch = RegexUtil::RegexMatch(data, pattern);
    EXPECT_EQ(smatch.value()[0], "AaaAaaaA");

    pattern = "[a-zA-z]+";
    smatch = RegexUtil::RegexMatch(data, pattern);
    EXPECT_EQ(smatch.value()[0], "AaaAaaaA");
}
