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
#include <map>
#include <set>
#include <string>
#include <vector>
#include "DbSummaryDataBase.h"

using Dic::Module::FullDb::DbSummaryDataBase;
using Dic::Protocol::OperatorDetailCmpInfoRes;
using Dic::Protocol::OperatorDetailInfoRes;

class DbSummaryDatabaseTest : public ::testing::Test {
  protected:
    const std::set<std::string> rawPmuHeaders_ = {
        "aic_mac_time", "aic_mac_ratio", "aic_scalar_time", "aic_scalar_ratio", "aic_mte1_time", "aic_mte1_ratio"};
    const std::set<std::string> expectedPmuHeaders_ = {"aic_mac_time(us)", "aic_mac_ratio", "aic_scalar_time(us)",
        "aic_scalar_ratio", "aic_mte1_time(us)", "aic_mte1_ratio"};
};

TEST_F(DbSummaryDatabaseTest, ConvertsTimeColumnsAndPreservesRatios) {
    std::set<std::string> displayHeaders;
    std::vector<OperatorDetailInfoRes> rows(1);
    rows.front().pmuDatas = {{"aic_mac_time", "2500"}, {"aic_mac_ratio", "0.75"}, {"aic_scalar_time", "1"},
        {"aic_scalar_ratio", "0.25"}, {"aic_mte1_time", "1234"}, {"aic_mte1_ratio", "0.5"}};

    DbSummaryDataBase::ConvertPmuTimeColumnsToUs(
        rawPmuHeaders_, displayHeaders, rows, [](auto &row) -> auto & { return row.pmuDatas; });

    const std::map<std::string, std::string> expectedData = {{"aic_mac_time(us)", "2.5"}, {"aic_mac_ratio", "0.75"},
        {"aic_scalar_time(us)", "0.001"}, {"aic_scalar_ratio", "0.25"}, {"aic_mte1_time(us)", "1.234"},
        {"aic_mte1_ratio", "0.5"}};
    EXPECT_EQ(displayHeaders, expectedPmuHeaders_);
    EXPECT_EQ(rows.front().pmuDatas, expectedData);
}

TEST_F(DbSummaryDatabaseTest, PreservesEmptyAndNonNumericValues) {
    std::set<std::string> displayHeaders;
    std::vector<OperatorDetailInfoRes> rows(1);
    rows.front().pmuDatas = {
        {"aic_mac_time", ""}, {"aic_scalar_time", "N/A"}, {"aic_mte1_time", "-1250"}, {"aic_mac_ratio", "0.75"}};

    DbSummaryDataBase::ConvertPmuTimeColumnsToUs(
        rawPmuHeaders_, displayHeaders, rows, [](auto &row) -> auto & { return row.pmuDatas; });

    const std::map<std::string, std::string> expectedData = {{"aic_mac_time(us)", ""}, {"aic_scalar_time(us)", "N/A"},
        {"aic_mte1_time(us)", "-1.25"}, {"aic_mac_ratio", "0.75"}};
    EXPECT_EQ(displayHeaders, expectedPmuHeaders_);
    EXPECT_EQ(rows.front().pmuDatas, expectedData);
}

TEST_F(DbSummaryDatabaseTest, ConvertsCompareRowsWithoutChangingBaseline) {
    std::set<std::string> displayHeaders;
    std::vector<OperatorDetailCmpInfoRes> rows(1);
    rows.front().compare.pmuDatas = {{"aic_mac_time", "3000"}, {"aic_mac_ratio", "0.5"}};
    rows.front().baseline.pmuDatas = {{"aic_mac_time", "4000"}};

    DbSummaryDataBase::ConvertPmuTimeColumnsToUs(
        rawPmuHeaders_, displayHeaders, rows, [](auto &row) -> auto & { return row.compare.pmuDatas; });

    EXPECT_EQ(displayHeaders, expectedPmuHeaders_);
    EXPECT_EQ(rows.front().compare.pmuDatas.at("aic_mac_time(us)"), "3");
    EXPECT_EQ(rows.front().compare.pmuDatas.at("aic_mac_ratio"), "0.5");
    EXPECT_EQ(rows.front().compare.pmuDatas.count("aic_mac_time"), 0);
    EXPECT_EQ(rows.front().baseline.pmuDatas.at("aic_mac_time"), "4000");
}

TEST_F(DbSummaryDatabaseTest, UpdatesHeadersWhenRowsAreEmpty) {
    std::set<std::string> displayHeaders = {"stale"};
    std::vector<OperatorDetailInfoRes> rows;

    DbSummaryDataBase::ConvertPmuTimeColumnsToUs(
        rawPmuHeaders_, displayHeaders, rows, [](auto &row) -> auto & { return row.pmuDatas; });

    EXPECT_EQ(displayHeaders, expectedPmuHeaders_);
    EXPECT_TRUE(rows.empty());
}
