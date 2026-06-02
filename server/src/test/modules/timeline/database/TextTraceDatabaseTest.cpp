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
