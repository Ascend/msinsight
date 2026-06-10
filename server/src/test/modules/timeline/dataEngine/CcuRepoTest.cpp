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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
#include <gtest/gtest.h>
#include "CcuRepo.h"
#include "DataBaseManager.h"
#include "FileUtil.h"
#include "TestSuit.h"

using namespace Dic::Module::Timeline;

class CcuRepoTest : public ::testing::Test {
  public:
    static std::string g_testDbPath;
    static std::recursive_mutex g_testMutex;
    static Module::Database g_testDataBase;

    static void SetUpTestSuite() {
        g_testDbPath = TestSuit::GetTestDataFile("test_ccu_database.db");
        g_testDataBase.OpenDb(g_testDbPath, false);
        DataBaseManager::Instance().SetDataType(DataType::DB, g_testDbPath);
        DataBaseManager::Instance().CreateTraceConnectionPool("0", g_testDbPath);
    }

    static void TearDownTestSuite() {
        g_testDataBase.CloseDb();
        if (FileUtil::CheckFilePathExist(g_testDbPath)) {
            FileUtil::RemoveFile(g_testDbPath);
        }
    }

    void SetUp() override {
        g_testDataBase.ExecSql("DROP TABLE IF EXISTS CCU;");
        g_testDataBase.ExecSql("DROP TABLE IF EXISTS STRING_IDS;");
        g_testDataBase.ExecSql("CREATE TABLE CCU(deviceId INTEGER, globalTaskId INTEGER, name INTEGER, "
                               "startNs INTEGER, endNs INTEGER, args INTEGER);");
        g_testDataBase.ExecSql("CREATE TABLE STRING_IDS(id INTEGER PRIMARY KEY, value TEXT);");
        DataBaseManager::Instance().SetDbPathMapping("0", g_testDbPath, "");
    }
};

std::string CcuRepoTest::g_testDbPath;
std::recursive_mutex CcuRepoTest::g_testMutex;
Module::Database CcuRepoTest::g_testDataBase(g_testMutex);

TEST_F(CcuRepoTest, QuerySliceDetailInfoFlattensArgsArray) {
    g_testDataBase.ExecSql("INSERT INTO STRING_IDS(id, value) VALUES "
                           "(1, 'release'), "
                           "(2, '[\"Physic Stream Id\",61,\"Task Id\",12,\"Instruction ID\",930]');");
    g_testDataBase.ExecSql("INSERT INTO CCU(deviceId, globalTaskId, name, startNs, endNs, args) VALUES "
                           "(0, 1, 1, 1780025684159342310, 1780025684159357350, 2);");

    CcuRepo ccuRepo;
    SliceQuery query;
    query.sliceId = "1";
    query.rankId = "0";
    CompeteSliceDomain slice;
    const bool result = ccuRepo.QuerySliceDetailInfo(query, slice);

    EXPECT_EQ(result, true);
    EXPECT_EQ(slice.name, "release");
    EXPECT_NE(slice.args.find("\"Physic Stream Id\":61"), std::string::npos);
    EXPECT_NE(slice.args.find("\"Task Id\":12"), std::string::npos);
    EXPECT_NE(slice.args.find("\"Instruction ID\":930"), std::string::npos);
    EXPECT_NE(slice.args.find("\"deviceId\":\"0\""), std::string::npos);
    EXPECT_NE(slice.args.find("\"globalTaskId\":\"1\""), std::string::npos);
    EXPECT_NE(slice.args.find("\"startNs\":\"1780025684159342310\""), std::string::npos);
    EXPECT_NE(slice.args.find("\"endNs\":\"1780025684159357350\""), std::string::npos);
    EXPECT_EQ(slice.args.find("\"args\""), std::string::npos);
}
