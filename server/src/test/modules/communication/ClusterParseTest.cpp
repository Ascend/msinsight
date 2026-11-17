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

class ClusterParserRollbackTest : public ::testing::Test {
  protected:
    class MemoryDatabase : public Dic::Module::TextClusterDatabase {
      public:
        explicit MemoryDatabase(std::recursive_mutex &sqlMutex) : TextClusterDatabase(sqlMutex) {}

        bool OpenMemory() {
            isOpen = sqlite3_open(":memory:", &db) == SQLITE_OK;
            path = ":memory:";
            return isOpen;
        }

        int64_t QueryInteger(const std::string &sql) {
            sqlite3_stmt *statement = nullptr;
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
                ADD_FAILURE() << sqlite3_errmsg(db);
                return -1;
            }
            int result = sqlite3_step(statement);
            EXPECT_EQ(result, SQLITE_ROW);
            int64_t value = result == SQLITE_ROW ? sqlite3_column_int64(statement, 0) : -1;
            sqlite3_finalize(statement);
            return value;
        }
    };

    void SetUp() override {
        database = std::make_shared<MemoryDatabase>(sqlMutex);
        ASSERT_TRUE(database->OpenMemory());
        ASSERT_TRUE(database->CreateTable());
        ASSERT_TRUE(database->InitStmt());
        uniqueKey = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        ASSERT_TRUE(ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::INIT));
        ASSERT_TRUE(ParserStatusManager::Instance().SetClusterParseStatus(uniqueKey, ParserStatus::RUNNING));
    }

    void TearDown() override {
        ParserStatusManager::Instance().EraseClusterParserStatusByKeyWord(uniqueKey);
        database.reset();
    }

    std::recursive_mutex sqlMutex;
    std::shared_ptr<MemoryDatabase> database;
    std::string uniqueKey;
};

TEST_F(ClusterParserRollbackTest, CommunicationPreservesInputTotalsAndCompletesLegacyGroupHashes) {
    Dic::Module::CommGroupParallelInfo group;
    group.rankSetStr = "(0, 1)";
    group.type = "collective";
    group.pgName = "legacy_pg";
    ASSERT_TRUE(database->InsertGroupInfos({group}));
    const int64_t originalGroupId = database->QueryInteger("SELECT id FROM group_id");
    const char *json = R"json({"(0, 1)":{"step1":{
        "hcom_allReduce_0@hash_a":{"0":{
            "Communication Time Info":{"Elapse Time(ms)":1},
            "Communication Bandwidth Info":{"RDMA":{
                "Transit Size(MB)":2,"Transit Time(ms)":1.25,
                "Size Distribution":{"1024":[2,1.25]}}}}},
        "hcom_allReduce_1@hash_b":{"0":{
            "Communication Time Info":{"Elapse Time(ms)":2},
            "Communication Bandwidth Info":{"RDMA":{
                "Transit Size(MB)":3,"Transit Time(ms)":2,"Size Distribution":{}}}}},
        "Total Op Info@hash_a":{"0":{
            "Communication Time Info":{"Elapse Time(ms)":99},
            "Communication Bandwidth Info":{"RDMA":{
                "Transit Size(MB)":99,"Transit Time(ms)":99,"Size Distribution":{}}}}}
    }}})json";
    rapidjson::StringStream stream(json);
    rapidjson::Reader reader;
    CommunicationRapidSaxHandler handler(database, uniqueKey);
    ASSERT_TRUE(reader.Parse<rapidjson::kParseNumbersAsStringsFlag>(stream, handler));
    database->SaveLastData();

    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM communication_time_info"), 3);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM communication_bandwidth_info"), 3);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(DISTINCT stage_id) FROM communication_time_info "
                                     "WHERE op_suffix != ''"),
        2);
    EXPECT_EQ(database->QueryInteger("SELECT elapse_time FROM communication_time_info "
                                     "WHERE op_name = 'Total Op Info'"),
        99);
    EXPECT_EQ(database->QueryInteger("SELECT transit_size FROM communication_bandwidth_info "
                                     "WHERE op_name = 'Total Op Info'"),
        99);
    EXPECT_EQ(database->QueryInteger(R"(SELECT COUNT(*) FROM communication_bandwidth_info
        WHERE size_distribution = '{"1024":[2,1.25]}')"),
        1);
    const auto rankTimes = database->GetCommTimeForRankDim("1");
    ASSERT_EQ(rankTimes.size(), 1);
    EXPECT_EQ(rankTimes[0].groupIdHash, "hash_a");
    EXPECT_DOUBLE_EQ(rankTimes[0].commTime, 99000);
    EXPECT_EQ(database->GetAllGroupInfo().size(), 2);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM group_id WHERE group_id_hash = 'hash_a'"), 1);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM group_id WHERE group_id_hash = 'hash_b'"), 1);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM group_id WHERE group_id_hash = ''"), 0);
    EXPECT_EQ(database->QueryInteger("SELECT id FROM group_id WHERE group_id_hash = 'hash_a'"), originalGroupId);
}

TEST_F(ClusterParserRollbackTest, MatrixPreservesInputTotalsAndCompletesLegacyGroupHashes) {
    Dic::Module::CommGroupParallelInfo group;
    group.rankSetStr = "(0, 1)";
    group.type = "collective";
    ASSERT_TRUE(database->InsertGroupInfos({group}));
    const int64_t originalGroupId = database->QueryInteger("SELECT id FROM group_id");
    const char *json = R"json({"(0, 1)":{"step1":{
        "hcom_allReduce_0@hash_a":{"0-1":{
            "Transport Type":"RDMA","Transit Size(MB)":2,"Transit Time(ms)":1,"Bandwidth(GB/s)":2}},
        "hcom_allReduce_1@hash_b":{"0-1":{
            "Transport Type":"RDMA","Transit Size(MB)":3,"Transit Time(ms)":1,"Bandwidth(GB/s)":3}},
        "Total Op Info@hash_a":{"0-1":{
            "Transport Type":"RDMA","Transit Size(MB)":99,"Transit Time(ms)":99,"Bandwidth(GB/s)":1}}
    }}})json";
    rapidjson::StringStream stream(json);
    rapidjson::Reader reader;
    CommunicationMatrixRapidHandler handler(database, uniqueKey);
    ASSERT_TRUE(reader.Parse(stream, handler));
    database->SaveLastData();

    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM communication_matrix"), 3);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(DISTINCT group_id) FROM communication_matrix "
                                     "WHERE group_name != ''"),
        2);
    EXPECT_EQ(database->QueryInteger("SELECT transit_size FROM communication_matrix "
                                     "WHERE op_sort = 'Total Op Info'"),
        99);
    EXPECT_EQ(database->GetAllGroupInfo().size(), 2);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM group_id WHERE group_id_hash = 'hash_a'"), 1);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM group_id WHERE group_id_hash = 'hash_b'"), 1);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM group_id WHERE group_id_hash = ''"), 0);
    EXPECT_EQ(database->QueryInteger("SELECT id FROM group_id WHERE group_id_hash = 'hash_a'"), originalGroupId);
}

TEST_F(ClusterParserRollbackTest, CommunicationReusesHashesCompletedByMatrix) {
    Dic::Module::CommGroupParallelInfo group;
    group.rankSetStr = "(0, 1)";
    group.type = "collective";
    group.pgName = "legacy_pg";
    ASSERT_TRUE(database->InsertGroupInfos({group}));
    const int64_t originalGroupId = database->QueryInteger("SELECT id FROM group_id");
    const char *matrixJson = R"json({"(0, 1)":{"step1":{
        "hcom_allReduce_0@hash_a":{"0-1":{
            "Transport Type":"RDMA","Transit Size(MB)":2,"Transit Time(ms)":1,"Bandwidth(GB/s)":2}}
    }}})json";
    rapidjson::StringStream matrixStream(matrixJson);
    rapidjson::Reader matrixReader;
    CommunicationMatrixRapidHandler matrixHandler(database, uniqueKey);
    ASSERT_TRUE(matrixReader.Parse(matrixStream, matrixHandler));
    database->SaveLastData();
    ASSERT_EQ(database->QueryInteger("SELECT id FROM group_id WHERE group_id_hash = 'hash_a'"), originalGroupId);

    const char *communicationJson = R"json({"(0, 1)":{"step1":{
        "hcom_allReduce_0@hash_a":{"0":{"Communication Time Info":{"Elapse Time(ms)":1}}},
        "hcom_allReduce_1@hash_b":{"0":{"Communication Time Info":{"Elapse Time(ms)":2}}}
    }}})json";
    rapidjson::StringStream communicationStream(communicationJson);
    rapidjson::Reader communicationReader;
    CommunicationRapidSaxHandler communicationHandler(database, uniqueKey);
    ASSERT_TRUE(
        communicationReader.Parse<rapidjson::kParseNumbersAsStringsFlag>(communicationStream, communicationHandler));
    database->SaveLastData();

    EXPECT_EQ(database->GetAllGroupInfo().size(), 2);
    EXPECT_EQ(database->QueryInteger("SELECT id FROM group_id WHERE group_id_hash = 'hash_a'"), originalGroupId);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM group_id "
                                     "WHERE group_id_hash = 'hash_a' AND pg_name = 'legacy_pg'"),
        1);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM communication_time_info t "
                                     "JOIN group_id g ON t.stage_id = g.id WHERE t.op_suffix = g.group_id_hash"),
        2);
    EXPECT_EQ(database->QueryInteger("SELECT COUNT(*) FROM communication_matrix m "
                                     "JOIN group_id g ON m.group_id = g.id WHERE m.group_name = g.group_id_hash"),
        1);
}
