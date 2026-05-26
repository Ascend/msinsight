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
#include "../../../TestSuit.h"
#include "Database.h"
#include "DataBaseManager.h"
#include "DbSummaryDataBase.h"
#include "GlobalProtocolResponse.h"
#include "TextSummaryDataBase.h"
#include "OperatorProtocolDefs.h"
#include "OperatorProtocolRequest.h"

using namespace Dic;
using namespace Dic::Module;
using namespace Dic::Module::Summary;

std::recursive_mutex db0Mutex;
std::recursive_mutex text0Mutex;

DbSummaryDataBase fullDbRank0 = DbSummaryDataBase(db0Mutex);
TextSummaryDataBase textDbRank0 = TextSummaryDataBase(text0Mutex);

class RequestGroup {
  public:
    static inline const std::string OP_TYPE = "Operator Type";
    static inline const std::string OP = "Operator";
    static inline const std::string INPUT_SHAPE = "Input Shape";
    static inline const std::string COMM_OP = "Communication Operator";
    static inline const std::string COMM_OP_TYPE = "Communication Operator Type";
};

class VirtualSummaryDatabaseTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        std::string operatorDataPath = TestSuit::GetTestDataFile("operator_st");
        std::string dbPathRank0 = FileUtil::SplicePath(operatorDataPath, "db_rank_0.dat");
        std::string textPathRank0 = FileUtil::SplicePath(operatorDataPath, "text_rank_0.dat");
        // 设置db版本避免被重置，该套件仅用于db/text场景查询ST，不考虑解析
        SetDatabaseVersion(dbPathRank0);
        SetDatabaseVersion(textPathRank0);
        // 初始化stringCache
        std::recursive_mutex tmpMutex;
        DbTraceDataBase tmpFullDbRank0 = DbTraceDataBase(tmpMutex);
        tmpFullDbRank0.SetDbPath(dbPathRank0);
        ASSERT_TRUE(tmpFullDbRank0.OpenDb(dbPathRank0, false));
        tmpFullDbRank0.InitStringsCache();
        tmpFullDbRank0.CloseDb();
        ASSERT_EQ(DbTraceDataBase::GetStringCacheValue(dbPathRank0, "202"), "N/A");

        fullDbRank0.SetDbPath(dbPathRank0);
        textDbRank0.SetDbPath(textPathRank0);

        ASSERT_TRUE(fullDbRank0.OpenDb(dbPathRank0, false));
        ASSERT_TRUE(textDbRank0.OpenDb(textPathRank0, false));
    }

    static void TearDownTestSuite() {
        fullDbRank0.CloseDb();
        textDbRank0.CloseDb();
    }

    static void SetDatabaseVersion(std::string &dbPath) {
        std::recursive_mutex mutex;
        Database db = Database(mutex);
        db.SetDbPath(dbPath);
        ASSERT_TRUE(db.OpenDb(dbPath, false));
        ASSERT_TRUE(db.SetDataBaseVersion());
        ASSERT_NO_THROW(db.CloseDb());
    }

    static bool CompareOperatorStatisticInfoRes(
        const OperatorStatisticInfoRes &res1, const OperatorStatisticInfoRes &res2) {
        return res1.count == res2.count && res1.avgTime == res2.avgTime && res1.maxTime == res2.maxTime &&
            res1.minTime == res2.minTime && res1.totalTime == res2.totalTime;
    }

    static bool CompareOperatorStatisticInfoResponse(
        const OperatorStatisticInfoResponse &resp1, const OperatorStatisticInfoResponse &resp2) {
        bool res = resp1.total == resp2.total;
        res = res && resp1.data.size() == resp2.data.size();
        if (!res) {
            return false;
        }
        size_t size = resp1.data.size();
        for (size_t i = 0; i < size; i++) {
            res = CompareOperatorStatisticInfoRes(resp1.data[i].baseline, resp2.data[i].baseline);
            if (!res) {
                return false;
            }
        }
        return true;
    }
};

TEST_F(VirtualSummaryDatabaseTest, QueryStatisticForComputeOperatorTypeWithNoFilter) {
    // 无过滤请求计算算子top15, 10分页
    OperatorStatisticReqParams commonQueryParams;
    commonQueryParams.group = RequestGroup::OP_TYPE;
    commonQueryParams.pageSize = 10;
    commonQueryParams.current = 1;
    commonQueryParams.deviceId = "0";
    commonQueryParams.topK = 15;
    OperatorStatisticInfoResponse textResponse = {};
    OperatorStatisticInfoResponse dbResponse = {};
    textDbRank0.QueryOperatorStatisticInfo(commonQueryParams, textResponse);
    fullDbRank0.QueryOperatorStatisticInfo(commonQueryParams, dbResponse);
    EXPECT_EQ(textResponse.data.size() + dbResponse.data.size(), commonQueryParams.pageSize * 2);
    EXPECT_TRUE(CompareOperatorStatisticInfoResponse(textResponse, dbResponse));
    // 无过滤请求计算算子top15, 100分页
    commonQueryParams.pageSize = 100;
    textResponse = {};
    dbResponse = {};
    textDbRank0.QueryOperatorStatisticInfo(commonQueryParams, textResponse);
    fullDbRank0.QueryOperatorStatisticInfo(commonQueryParams, dbResponse);
    EXPECT_EQ(textResponse.data.size() + dbResponse.data.size(), commonQueryParams.topK * 2);
    EXPECT_TRUE(CompareOperatorStatisticInfoResponse(textResponse, dbResponse));
    // 无过滤请求计算算子topAll, 100分页
    commonQueryParams.topK = -1;
    textResponse = {};
    dbResponse = {};
    textDbRank0.QueryOperatorStatisticInfo(commonQueryParams, textResponse);
    fullDbRank0.QueryOperatorStatisticInfo(commonQueryParams, dbResponse);
    EXPECT_EQ(textResponse.data.size() + dbResponse.data.size(), textResponse.total * 2);
    EXPECT_TRUE(CompareOperatorStatisticInfoResponse(textResponse, dbResponse));
    // 无过滤请求计算算子topAll, 10分页
    commonQueryParams.pageSize = 10;
    textResponse = {};
    dbResponse = {};
    textDbRank0.QueryOperatorStatisticInfo(commonQueryParams, textResponse);
    fullDbRank0.QueryOperatorStatisticInfo(commonQueryParams, dbResponse);
    EXPECT_EQ(textResponse.data.size() + dbResponse.data.size(), commonQueryParams.pageSize * 2);
    EXPECT_TRUE(CompareOperatorStatisticInfoResponse(textResponse, dbResponse));
}

// 测试所有可排序列的在top15, 10分页的情况下的db和text; 暂无实际的排序是否按顺序的验证手段，后续重构加入排序结果验证UT
TEST_F(VirtualSummaryDatabaseTest, QueryStatisticForComputeOperatorTypeWithWithAllOrderCol) {
    // 请求计算算子top15, 10分页
    OperatorStatisticReqParams commonQueryParams;
    commonQueryParams.group = RequestGroup::OP_TYPE;
    commonQueryParams.pageSize = 10;
    commonQueryParams.current = 1;
    commonQueryParams.deviceId = "0";
    commonQueryParams.order = "desc";
    commonQueryParams.topK = 15;
    OperatorStatisticInfoResponse textResponse;
    OperatorStatisticInfoResponse dbResponse;
    for (auto orderCol : OperatorStatisticView::VALID_ORDER_COLS) {
        commonQueryParams.orderBy = std::string(orderCol);
        textResponse = {};
        dbResponse = {};
        textDbRank0.QueryOperatorStatisticInfo(commonQueryParams, textResponse);
        fullDbRank0.QueryOperatorStatisticInfo(commonQueryParams, dbResponse);
        EXPECT_FALSE(textResponse.data.empty());
        EXPECT_EQ(textResponse.data.size() + dbResponse.data.size(), commonQueryParams.pageSize * 2);
        EXPECT_TRUE(CompareOperatorStatisticInfoResponse(textResponse, dbResponse));
    }
}

/***
 * 以下为operator/statistic接口的过滤测试, 包含计算算子类型、计算算子名及输入shape、通信算子类型三个分组
 */
// 计算算子类型分组时，可过滤type,accCore  请求计算算子topALL, 100分页
TEST_F(VirtualSummaryDatabaseTest, QueryStatisticForComputeOperatorTypeWithWithFilter) {
    OperatorStatisticReqParams commonQueryParams;
    commonQueryParams.group = RequestGroup::OP_TYPE;
    commonQueryParams.pageSize = 10;
    commonQueryParams.current = 1;
    commonQueryParams.deviceId = "0";
    commonQueryParams.orderBy = OperatorStatisticView::OP_NAME;
    commonQueryParams.order = "desc";
    commonQueryParams.topK = -1;
    commonQueryParams.filters = {
        {std::string(OperatorStatisticView::OP_TYPE), "mul"},
        {std::string(OperatorStatisticView::ACC_CORE), "VECTOR"},
    };
    OperatorStatisticInfoResponse textResponse = {};
    OperatorStatisticInfoResponse dbResponse = {};
    textDbRank0.QueryOperatorStatisticInfo(commonQueryParams, textResponse);
    fullDbRank0.QueryOperatorStatisticInfo(commonQueryParams, dbResponse);
    EXPECT_FALSE(textResponse.data.empty());
    EXPECT_EQ(
        textResponse.data.size() + dbResponse.data.size(), min(textResponse.total, commonQueryParams.pageSize) * 2);
    EXPECT_TRUE(CompareOperatorStatisticInfoResponse(textResponse, dbResponse));
}
// 计算算子名及输入shape分组时，可过滤name和accCore
TEST_F(VirtualSummaryDatabaseTest, QueryStatisticForComputeOperatorNameAndInputShapeWithFilter) {
    OperatorStatisticReqParams commonQueryParams;
    commonQueryParams.group = RequestGroup::INPUT_SHAPE;
    commonQueryParams.pageSize = 10;
    commonQueryParams.current = 1;
    commonQueryParams.deviceId = "0";
    commonQueryParams.orderBy = OperatorStatisticView::OP_NAME;
    commonQueryParams.order = "desc";
    commonQueryParams.topK = -1;
    commonQueryParams.filters = {
        {std::string(OperatorStatisticView::OP_NAME), "Inplace"},
        {std::string(OperatorStatisticView::ACC_CORE), "VECTOR"},
    };
    OperatorStatisticInfoResponse textResponse = {};
    OperatorStatisticInfoResponse dbResponse = {};
    textDbRank0.QueryOperatorStatisticInfo(commonQueryParams, textResponse);
    fullDbRank0.QueryOperatorStatisticInfo(commonQueryParams, dbResponse);
    EXPECT_FALSE(textResponse.data.empty());
    EXPECT_EQ(
        textResponse.data.size() + dbResponse.data.size(), min(textResponse.total, commonQueryParams.pageSize) * 2);
    EXPECT_TRUE(CompareOperatorStatisticInfoResponse(textResponse, dbResponse));
}
// 通信算子类型分组时, 可过滤type
TEST_F(VirtualSummaryDatabaseTest, QueryStatisticForCommunicationOpTypeWithFilter) {
    OperatorStatisticReqParams commonQueryParams;
    commonQueryParams.group = RequestGroup::COMM_OP_TYPE;
    commonQueryParams.pageSize = 10;
    commonQueryParams.current = 1;
    commonQueryParams.deviceId = "0";
    commonQueryParams.orderBy = OperatorStatisticView::OP_NAME;
    commonQueryParams.order = "desc";
    commonQueryParams.topK = -1;
    commonQueryParams.filters = {
        {std::string(OperatorStatisticView::OP_TYPE), "gather"},
    };
    OperatorStatisticInfoResponse textResponse = {};
    OperatorStatisticInfoResponse dbResponse = {};
    textDbRank0.QueryOperatorStatisticInfo(commonQueryParams, textResponse);
    fullDbRank0.QueryOperatorStatisticInfo(commonQueryParams, dbResponse);
    EXPECT_FALSE(textResponse.data.empty());
    EXPECT_EQ(
        textResponse.data.size() + dbResponse.data.size(), min(textResponse.total, commonQueryParams.pageSize) * 2);
    EXPECT_TRUE(CompareOperatorStatisticInfoResponse(textResponse, dbResponse));
}
