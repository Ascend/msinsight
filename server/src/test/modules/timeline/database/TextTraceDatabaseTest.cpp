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
#include "utils/Utf8TestUtil.h"
#include <gtest/gtest.h>
#include "TextTraceDatabase.h"

class TextTraceDatabaseTest : public testing::Test {};

TEST_F(TextTraceDatabaseTest, OpenDb) {
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    database.OpenDb("tttt", true);
    database.CloseDb();
}

/**
 * db路径有日志注入
 */
TEST_F(TextTraceDatabaseTest, TestOpenDbWithPathInject) {
    std::recursive_mutex sqlMutex;
    Dic::Module::Database database(sqlMutex);
    bool result = database.OpenDb("tt>>>><<</\tt", true);
    EXPECT_EQ(result, false);
    database.CloseDb();
}

/**
 * db路径超过最大限制
 */
TEST_F(TextTraceDatabaseTest, TestOpenDbWithPathLengthExceedLimit) {
    std::recursive_mutex sqlMutex;
    Dic::Module::Database database(sqlMutex);
    std::string dbPath;
    int lengthLimit = 0;
#ifdef _WIN32
    const uint32_t WIN_FILE_WRONG_LENGTH = 261;
    lengthLimit = WIN_FILE_WRONG_LENGTH;
#else
    const uint32_t LINUX_FILE_WRONG_LENGTH = 4097;
    lengthLimit = LINUX_FILE_WRONG_LENGTH;
#endif
    for (int i = 0; i < lengthLimit; ++i) {
        dbPath += "a";
    }
    bool result = database.OpenDb(dbPath, true);
    EXPECT_EQ(result, false);
    database.CloseDb();
}

TEST_F(TextTraceDatabaseTest, InitStmt) {
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    bool success = database.InitStmt();
    EXPECT_EQ(success, false);
}

TEST_F(TextTraceDatabaseTest, CreateTable) {
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    database.CreateTable();

    database.OpenDb("tttt", true);
    bool success = database.CreateTable();
    database.CloseDb();
    EXPECT_EQ(success, true);
}

TEST_F(TextTraceDatabaseTest, DropTable) {
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    bool success = database.DropTable();
    EXPECT_EQ(success, false);
}

TEST_F(TextTraceDatabaseTest, CreateIndex) {
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    database.CreateIndex();

    database.OpenDb("tttt", true);
    ASSERT_TRUE(database.CreateTable());
    bool success = database.CreateIndex();
    database.CloseDb();
    EXPECT_EQ(success, true);
}

TEST_F(TextTraceDatabaseTest, InsertSlice) {
    std::recursive_mutex sqlMutex;
    const int size = 1000;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    bool success = true;
    for (int i = 0; i < size; i++) {
        Dic::Module::Timeline::Trace::Slice event;
        success = success && database.InsertSlice(event);
    }
    EXPECT_EQ(success, true);
}

TEST_F(TextTraceDatabaseTest, InsertFlow) {
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    bool success = true;
    const int size = 1000;
    for (int i = 0; i < size; i++) {
        Dic::Module::Timeline::Trace::Flow event;
        success = success && database.InsertFlow(event);
    }
    EXPECT_EQ(success, true);
}

TEST_F(TextTraceDatabaseTest, InsertCounter) {
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    bool success = true;
    const int size = 1000;
    for (int i = 0; i < size; i++) {
        Dic::Module::Timeline::Trace::Counter event;
        success = success && database.InsertCounter(event);
    }
    EXPECT_EQ(success, true);
}

TEST_F(TextTraceDatabaseTest, Utf8ThreadAndCounterNamesRemainUnchanged) {
    Dic::Test::Utf8TempDirectory directory;
    std::recursive_mutex mutex;
    Dic::Module::Timeline::TextTraceDatabase database(mutex);
    ASSERT_TRUE(database.Database::OpenDb(directory.Path("trace.db"), false));
    ASSERT_TRUE(database.ExecSql("CREATE TABLE process (pid TEXT, process_name TEXT, label TEXT, "
                                 "process_sort_index INTEGER, parentPid TEXT)"));
    ASSERT_TRUE(database.ExecSql("INSERT INTO process VALUES ('3', 'process', '', 0, '')"));
    ASSERT_TRUE(database.ExecSql("CREATE TABLE thread (track_id INTEGER, tid TEXT, pid TEXT, "
                                 "thread_name TEXT, thread_sort_index INTEGER)"));
    ASSERT_TRUE(database.ExecSql("CREATE TABLE counter (pid TEXT, name TEXT, args TEXT)"));
    ASSERT_TRUE(database.ExecSql("CREATE TABLE slice (track_id INTEGER, depth INTEGER, cat TEXT)"));
    const std::string threadName = u8"练 😀 thread";
    const std::string counterName = u8"练 😀 counter";
    {
        auto insert = database.CreatPreparedStatement("INSERT INTO thread VALUES (1, '2', '3', ?, 0)");
        ASSERT_NE(insert, nullptr);
        ASSERT_TRUE(insert->Execute(threadName));
    }
    {
        auto insert = database.CreatPreparedStatement("INSERT INTO thread VALUES (2, '4', '3', ?, 1)");
        ASSERT_NE(insert, nullptr);
        ASSERT_TRUE(insert->Execute(counterName));
    }
    {
        auto insert = database.CreatPreparedStatement("INSERT INTO counter VALUES ('3', ?, '{}')");
        ASSERT_NE(insert, nullptr);
        ASSERT_TRUE(insert->Execute(counterName));
    }
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metadata;
    ASSERT_TRUE(database.QueryUnitsMetadata("utf8", metadata));
    std::vector<std::string> names;
    std::function<void(const Dic::Protocol::UnitTrack &)> collect = [&](const auto &unit) {
        names.push_back(unit.metaData.threadName);
        for (const auto &child : unit.children) {
            collect(*child);
        }
    };
    for (const auto &unit : metadata) {
        collect(*unit);
    }
    EXPECT_NE(std::find(names.begin(), names.end(), threadName), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), counterName), names.end());
}
