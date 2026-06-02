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
#include "ClusterFileParser.h"
#include "DbClusterDataBase.h"
#include "CommunicationMatrixRapidHandler.h"
#include "CommunicationRapidSaxHandler.h"
#include "../../TestSuit.h"

class ClusterParseTest : public ::testing::Test {};

TEST_F(ClusterParseTest, ParserDbClusterFailWithEmptyPath) {
    std::recursive_mutex mutex;
    std::string clusterDbPath;
    auto database = std::make_shared<Dic::Module::FullDb::DbClusterDataBase>(mutex);
    ClusterFileParser clusterFileParser(clusterDbPath, database, "Test");
    bool res = clusterFileParser.ParserClusterOfDb();
    EXPECT_EQ(res, false);
}

TEST_F(ClusterParseTest, MatrixRapidHandlerTest) {
    CommunicationMatrixRapidHandler handler(nullptr, "TEST");
    constexpr rapidjson::SizeType elementCount{};
    EXPECT_EQ(handler.Null(), true);
    EXPECT_EQ(handler.Bool(true), true);
    EXPECT_EQ(handler.Int(0), true);
    EXPECT_EQ(handler.Uint(0), true);
    EXPECT_EQ(handler.Int64(0), true);
    EXPECT_EQ(handler.Uint64(0), true);
    EXPECT_EQ(handler.StartArray(), true);
    EXPECT_EQ(handler.EndArray(elementCount), true);
}

TEST_F(ClusterParseTest, RapidSaxHandlerTest) {
    CommunicationRapidSaxHandler handler(nullptr, "TEST");
    EXPECT_EQ(handler.Null(), true);
    EXPECT_EQ(handler.Bool(true), true);
    EXPECT_EQ(handler.Int(0), true);
    EXPECT_EQ(handler.Uint(0), true);
    EXPECT_EQ(handler.Int64(0), true);
    EXPECT_EQ(handler.Uint64(0), true);
    EXPECT_EQ(handler.Double(0), true);
}
