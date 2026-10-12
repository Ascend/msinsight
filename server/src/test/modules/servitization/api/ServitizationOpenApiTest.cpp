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
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include "FileUtil.h"
#include "IEContext.h"
#include "ParserStatusManager.h"
#include "ServitizationOpenApi.h"
#include "ServitizationContext.h"
using namespace Dic::Module::IE;
class ServitizationOpenApiTest : public ::testing::Test {
  public:
    void SetUp() override { Dic::Module::Timeline::ParserStatusManager::Instance().NotifyStartParse(); }
};

TEST_F(ServitizationOpenApiTest, InitDataBaseTest) {
    std::shared_ptr<ServitizationOpenApi> openApi = std::make_shared<ServitizationOpenApi>();
    std::string folder = Dic::FileUtil::GetCurrPath();
    std::string tempFileName = Dic::FileUtil::SplicePath(folder, "profiler.db");
    std::string tempErrFileName = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "mmmm.text");
    Dic::Server::ServerLog::Info(tempFileName);
    std::ofstream tempFile(tempFileName, std::ios::out | std::ios::trunc);
    std::ofstream tempErrFile(tempErrFileName, std::ios::out | std::ios::trunc);
    auto res = openApi->ComputeTaskInfo(folder);
    EXPECT_EQ(res.size(), 1); // 1
    openApi->Reset();
}

TEST_F(ServitizationOpenApiTest, ParseTest) {
    std::shared_ptr<ServitizationOpenApi> openApi = std::make_shared<ServitizationOpenApi>();
    std::string folder = Dic::FileUtil::GetCurrPath();
    std::string tempFileName = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "profiler.db");
    std::string tempErrFileName = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "mmmm.text");
    std::ofstream tempFile(tempFileName, std::ios::out | std::ios::trunc);
    std::ofstream tempErrFile(tempErrFileName, std::ios::out | std::ios::trunc);
    std::unordered_map<std::string, std::string> inputs;
    inputs["lll"] = folder;
    auto res = openApi->Parse(inputs);
    EXPECT_EQ(res, false);
    openApi->Reset();
    inputs["lll"] = tempErrFileName;
    res = openApi->Parse(inputs);
    EXPECT_EQ(res, true);
    openApi->Reset();
    inputs["lll"] = tempFileName;
    res = openApi->Parse(inputs);
    EXPECT_EQ(res, true);
    openApi->Reset();
}

TEST_F(ServitizationOpenApiTest, ParseMsTest) {
    std::shared_ptr<ServitizationOpenApi> openApi = std::make_shared<ServitizationOpenApi>();
    std::string folder = Dic::FileUtil::GetCurrPath();
    std::string tempFileName = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "ms_service_parsed.db");
    std::string tempFile1Name = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "ms_service_111.db");
    std::ofstream tempFile(tempFileName, std::ios::out | std::ios::trunc);
    std::ofstream tem2pFile(tempFile1Name, std::ios::out | std::ios::trunc);
    std::unordered_map<std::string, std::string> inputs;
    inputs["lll"] = folder;
    auto res = openApi->Parse(inputs);
    EXPECT_EQ(res, false);
    openApi->Reset();
    inputs["lll"] = tempFileName;
    res = openApi->Parse(inputs);
    EXPECT_EQ(res, true);
    openApi->Reset();
}

class ServitizationOpenApiFileTest : public ServitizationOpenApiTest {
  protected:
    void SetUp() override {
        ServitizationOpenApiTest::SetUp();
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        folder = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "ie_open_api_" + std::to_string(suffix));
        ASSERT_TRUE(std::filesystem::create_directory(folder));
    }

    void TearDown() override {
        IEContext::Instance().Reset();
        std::error_code error;
        std::filesystem::remove_all(folder, error);
        EXPECT_FALSE(error);
    }

    static bool CreateDatabase(const std::string &path, const char *sql) {
        sqlite3 *db = nullptr;
        const int openResult = sqlite3_open(path.c_str(), &db);
        if (openResult != SQLITE_OK) {
            sqlite3_close(db);
            return false;
        }
        const int execResult = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
        const int closeResult = sqlite3_close(db);
        return execResult == SQLITE_OK && closeResult == SQLITE_OK;
    }

    static int ReadInteger(const std::string &path, const char *sql) {
        sqlite3 *db = nullptr;
        if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            sqlite3_close(db);
            return -1;
        }
        sqlite3_stmt *statement = nullptr;
        int result = -1;
        if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK &&
            sqlite3_step(statement) == SQLITE_ROW) {
            result = sqlite3_column_int(statement, 0);
        }
        sqlite3_finalize(statement);
        sqlite3_close(db);
        return result;
    }

    std::string folder;
};

TEST_F(ServitizationOpenApiFileTest, FindsDirectAndNestedProfilerFiles) {
    const auto direct = Dic::FileUtil::SplicePath(folder, "profiler.db");
    const auto nestedFolder = Dic::FileUtil::SplicePath(folder, "nested");
    ASSERT_TRUE(std::filesystem::create_directory(nestedFolder));
    const auto nested = Dic::FileUtil::SplicePath(nestedFolder, "ms_service_parsed.db");
    std::ofstream(direct) << "data";
    std::ofstream(nested) << "data";

    ServitizationOpenApi api;
    const auto directTasks = api.ComputeTaskInfo(direct);
    ASSERT_EQ(directTasks.size(), 1);
    EXPECT_EQ(directTasks[0].filePath, direct);
    EXPECT_EQ(directTasks[0].fileId, Dic::FileUtil::GetFileName(folder) + "_0");

    const auto tasks = api.ComputeTaskInfo(folder);
    ASSERT_EQ(tasks.size(), 2);
    EXPECT_TRUE(std::any_of(tasks.begin(), tasks.end(), [&](const TaskInfo &task) { return task.filePath == direct; }));
    EXPECT_TRUE(std::any_of(tasks.begin(), tasks.end(), [&](const TaskInfo &task) { return task.filePath == nested; }));
}

TEST_F(ServitizationOpenApiFileTest, CreatesCurveOnlyForValidCurveScript) {
    const auto dbPath = Dic::FileUtil::SplicePath(folder, "profiler.db");
    ASSERT_TRUE(CreateDatabase(dbPath, "CREATE TABLE initial_data(id INTEGER);"));
    ASSERT_TRUE(IEContext::Instance().InitDataBase("rank", dbPath));

    ServitizationOpenApi api;
    EXPECT_FALSE(api.CreateCurve("rank", "CREATE TABLE invalid(id INTEGER);"));
    EXPECT_TRUE(api.CreateCurve("rank", "CREATE TABLE 'sample_curve' AS SELECT 42 AS amount;"));
    api.Reset();
    EXPECT_EQ(ReadInteger(dbPath, "SELECT amount FROM sample_curve;"), 42);
}

TEST_F(ServitizationOpenApiFileTest, MergesDistributedServiceTables) {
    const auto parsed = Dic::FileUtil::SplicePath(folder, "ms_service_parsed.db");
    const auto distributed = Dic::FileUtil::SplicePath(folder, "ms_service_111.db");
    ASSERT_TRUE(CreateDatabase(parsed, "CREATE TABLE metadata(value INTEGER);"));
    ASSERT_TRUE(CreateDatabase(distributed,
        "CREATE TABLE slice(id INTEGER); INSERT INTO slice VALUES(7);"
        "CREATE TABLE counter(id INTEGER); INSERT INTO counter VALUES(8);"
        "CREATE TABLE thread(id INTEGER); INSERT INTO thread VALUES(9);"));

    ServitizationOpenApi api;
    EXPECT_TRUE(api.Parse({{"rank", parsed}}));
    api.Reset();

    EXPECT_EQ(ReadInteger(parsed, "SELECT id FROM slice;"), 7);
    EXPECT_EQ(ReadInteger(parsed, "SELECT id FROM counter;"), 8);
    EXPECT_EQ(ReadInteger(parsed, "SELECT id FROM thread;"), 9);
}
