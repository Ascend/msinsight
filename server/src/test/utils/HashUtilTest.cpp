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
 * -------------------------------------------------------------------------
 */

#include <gtest/gtest.h>
#include "FileUtil.h"
#include "HashUtil.h"

using namespace Dic;

TEST(HashUtilTest, CalculateFileSha256WithSalt) {
    const std::string path = "hash_util_test.txt";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "snapshot-content";
    }
    EXPECT_EQ("811046db6def4f1c5e2ede99b96c23b47221722af51c0484d2251c38bc8f3718",
        HashUtil::CalculateFileSha256(path, "mem_snapshot_parser_v1"));
    FileUtil::RemoveFile(path);
}
