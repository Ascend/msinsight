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

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <gtest/gtest.h>
#include "CollectionUtil.h"

using namespace Dic;

TEST(CollectionUtilTest, FindValueByKeyReturnsValueOrDefault) {
    std::map<std::string, int> info = {{"rank0", 0}, {"rank1", 1}};

    EXPECT_EQ(CollectionUtil::FindValueByKey(info, "rank1", -1), 1);
    EXPECT_EQ(CollectionUtil::FindValueByKey(info, "rank2", -1), -1);
}

TEST(CollectionUtilTest, IsEleInContainerChecksStringMembership) {
    std::vector<std::string> container = {"trace_view.json", "mindstudio_insight_data.db"};

    EXPECT_TRUE(CollectionUtil::IsEleInContainer("trace_view.json", container));
    EXPECT_FALSE(CollectionUtil::IsEleInContainer("operator_memory.csv", container));
}

TEST(CollectionUtilTest, CalDifferenceVectorPreservesSourceOrder) {
    std::vector<std::string> source = {"rank0", "rank1", "rank0", "rank2"};
    std::vector<std::string> sub = {"rank1"};
    std::vector<std::string> expected = {"rank0", "rank0", "rank2"};

    EXPECT_EQ(CollectionUtil::CalDifferenceVector(source, sub), expected);
}

TEST(CollectionUtilTest, CalIntersectionUsesSecondVectorOrder) {
    std::vector<std::string> vec1 = {"rank0", "rank1", "rank1"};
    std::vector<std::string> vec2 = {"rank1", "rank2", "rank0", "rank1"};
    std::vector<std::string> expected = {"rank1", "rank0"};

    EXPECT_EQ(CollectionUtil::CalIntersection(vec1, vec2), expected);
}

TEST(CollectionUtilTest, IsVectorEqualIgnoreOrderCountsDuplicates) {
    std::vector<int> first = {1, 2, 2, 3};
    std::vector<int> same = {3, 2, 1, 2};
    std::vector<int> different = {3, 2, 1, 1};

    EXPECT_TRUE(CollectionUtil::IsVectorEqualIgnoreOrder(first, same));
    EXPECT_FALSE(CollectionUtil::IsVectorEqualIgnoreOrder(first, different));
}
