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
#include "MemScopeEventTree.h"

using namespace Dic::Module::MemScope;

TEST(MemScopeEventTreeTest, ResolvesFixedDynamicAndUnknownTagNames) {
    EXPECT_EQ(MemScopeMemoryDetailTreeNode::GetNodeNameByTag("PTA"), MEM_SCOPE_ALLOC_OWNER_PTA_NAME);
    EXPECT_EQ(MemScopeMemoryDetailTreeNode::GetNodeNameByTag("PTA@ops@aten@custom"), "@custom");
    EXPECT_EQ(MemScopeMemoryDetailTreeNode::GetNodeNameByTag("UNKNOWN"), MEM_SCOPE_ALLOC_OWNER_DEFAULT_NAME);
}

TEST(MemScopeEventTreeTest, BuildsMultipleRootsAndSkipsMissingParents) {
    const std::set<std::string> tags = {"", "CANN", "PTA", "PTA@model", "PTA@model@weight", "PTA@missing@child"};
    const auto roots = MemScopeMemoryDetailTreeNode::BuildForestByOrderedTags(tags);

    ASSERT_EQ(roots.size(), 2);
    EXPECT_EQ(roots[0]->tag, "CANN");
    EXPECT_EQ(roots[1]->tag, "PTA");
    ASSERT_EQ(roots[1]->children.size(), 1);
    EXPECT_EQ(roots[1]->children[0]->tag, "PTA@model");
    ASSERT_EQ(roots[1]->children[0]->children.size(), 1);
    EXPECT_EQ(roots[1]->children[0]->children[0]->name, MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_WEIGHT_NAME);
}

TEST(MemScopeEventTreeTest, EmptyTagsProduceEmptyForest) {
    EXPECT_TRUE(MemScopeMemoryDetailTreeNode::BuildForestByOrderedTags({""}).empty());
}
