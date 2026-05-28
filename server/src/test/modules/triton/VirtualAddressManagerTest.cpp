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
#include "allocator/VirtualAddressManager.h"
#include "TritonMemoryDefs.h"

using namespace Dic::Module::Triton;

class VirtualAddressManagerTest : public ::testing::Test {
  protected:
    VirtualAddressManager manager;

    void SetUp() override { manager.Reset(); }

    void TearDown() override { manager.Reset(); }
};

/**
 * @brief 场景说明：测试 Allocate 和 Free 的基本功能。
 */
TEST_F(VirtualAddressManagerTest, AllocateAndFreeTest) {
    uint64_t addr1 = manager.Allocate(100);
    EXPECT_EQ(addr1, 0); // 首个分配应该从 0 开始

    uint64_t addr2 = manager.Allocate(200);
    EXPECT_EQ(addr2, 100);

    // 释放第一块内存
    manager.Free(addr1, 100);

    // 再分配一块，应该复用刚刚释放的内存
    uint64_t addr3 = manager.Allocate(50);
    EXPECT_EQ(addr3, 0);

    // 再次分配一块 50 的，应该继续使用刚才剩下的 free block
    uint64_t addr4 = manager.Allocate(50);
    EXPECT_EQ(addr4, 50);

    // 再次分配，超出现有的 free blocks，会在 top address 处分配
    uint64_t addr5 = manager.Allocate(100);
    EXPECT_EQ(addr5, 300);
}

/**
 * @brief 场景说明：测试 ManageRecord 对 TritonRecord 中 block 的处理。
 */
TEST_F(VirtualAddressManagerTest, ManageRecordTest) {
    TritonRecord record;
    TritonTensorSegment seg1;

    TritonTensorBlock block1;
    block1.start = 10;
    block1.end = 20;
    block1.size = 100;

    TritonTensorBlock block2;
    block2.start = 15; // 和 block1 时间上有重叠
    block2.end = 30;
    block2.size = 200;

    TritonTensorBlock block3;
    block3.start = 25; // 在 block1 结束后开始
    block3.end = 40;
    block3.size = 150;

    seg1.blocks.push_back(block1);
    seg1.blocks.push_back(block2);
    seg1.blocks.push_back(block3);

    record.segments.push_back(seg1);

    manager.ManageRecord(record);

    // 验证分配的虚拟地址
    // 按照时间顺序：
    // TS=10: Allocate block1 (size 100) -> addr=0
    // TS=15: Allocate block2 (size 200) -> addr=100
    // TS=20: Free block1 (addr=0, size=100)
    // TS=25: Allocate block3 (size 150) -> addr=0 容量不够，应该在 top_address 300 分配？还是？
    // Wait, FreeBlock 是 (0, 100)，新的是 150，放不下，所以会在 current_top_address_ = 300 分配

    EXPECT_EQ(record.segments[0].blocks[0].virtualAddress, 0);
    EXPECT_EQ(record.segments[0].blocks[1].virtualAddress, 100);
    EXPECT_EQ(record.segments[0].blocks[2].virtualAddress, 300);
}
