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
#include "TextClusterDatabase.h"
#include "utils/Utf8TestUtil.h"
#include "DbClusterDataBase.h"
#include "CommunicationMatrixRapidHandler.h"
#include "CommunicationRapidSaxHandler.h"
#include "ParserStatusManager.h"
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

TEST_F(ClusterParseTest, Utf8CommunicationFilePathAndOperatorNameArePreserved) {
    Dic::Test::Utf8TempDirectory directory;
    const auto jsonPath = directory.Path(u8"练 😀.json");
    const std::string json = u8R"JSON({
        "p2p": {"step1": {"练😀@1": {"0": {
            "Communication Time Info": {
                "Start Timestamp(us)": 1, "Elapse Time(ms)": 2, "Transit Time(ms)": 1
            }
        }}}}
    })JSON";
    ASSERT_TRUE(Dic::Test::Utf8TempDirectory::Write(jsonPath, json));
    std::recursive_mutex mutex;
    auto database = std::make_shared<Dic::Module::TextClusterDatabase>(mutex);
    ASSERT_TRUE(database->OpenDb(directory.Path("cluster.db"), false));
    ASSERT_TRUE(database->CreateTable());
    ASSERT_TRUE(database->InitStmt());
    const std::string key = "utf8-communication";
    ASSERT_TRUE(ParserStatusManager::Instance().SetClusterParseStatus(key, ParserStatus::INIT));
    struct ClearStatus {
        std::string key;
        ~ClearStatus() { ParserStatusManager::Instance().EraseClusterParserStatusByKeyWord(key); }
    } status{key};
    ClusterFileParser parser(directory.Path(), database, key);
    parser.ParseCommunication({jsonPath});
    database->SaveLastData();
    auto statement = database->CreatPreparedStatement("SELECT op_name FROM communication_time_info WHERE op_name = ?");
    ASSERT_NE(statement, nullptr);
    auto result = statement->ExecuteQuery(std::string(u8"练😀"));
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->Next());
    EXPECT_EQ(result->GetString("op_name"), u8"练😀");
}
