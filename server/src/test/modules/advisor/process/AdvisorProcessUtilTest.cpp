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
#include "AdvisorProcessUtil.h"

using namespace Dic::Module::Advisor;
class AdvisorProcessUtilTest : public ::testing::Test {
  protected:
    std::vector<Dic::Protocol::FlowLocation> data = {
        {"1id", "id", "meta", "0", 0, 5, 1, "Pid", "A", "0", "type", "note"},
        {"tid", "id", "meta", "0", 0, 7, 2, "pid", "a", "0", "type", "note"},
        {"Tid", "id", "meta", "0", 0, 1, 3, "1id", "1", "0", "type", "note"}};
};

TEST_F(AdvisorProcessUtilTest, SortFlowLocationDataTestOrderByName) {
    Dic::Protocol::KernelDetailsParams param{};
    param.orderBy = "name";
    param.order = "ASC";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "1");
    EXPECT_EQ(data.at(data.size() - 1).name, "a");

    param.order = "asc";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "a");
    EXPECT_EQ(data.at(data.size() - 1).name, "1");

    param.order = "others";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "a");
    EXPECT_EQ(data.at(data.size() - 1).name, "1");

    param.order = "";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "a");
    EXPECT_EQ(data.at(data.size() - 1).name, "1");
}

TEST_F(AdvisorProcessUtilTest, SortFlowLocationDataTestOrderByDuration) {
    Dic::Protocol::KernelDetailsParams param{};
    param.orderBy = "duration";
    param.order = "ASC";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "A");
    EXPECT_EQ(data.at(data.size() - 1).name, "1");

    param.order = "asc";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "1");
    EXPECT_EQ(data.at(data.size() - 1).name, "a");
}

TEST_F(AdvisorProcessUtilTest, SortFlowLocationDataTestOrderByPid) {
    Dic::Protocol::KernelDetailsParams param{};
    param.orderBy = "pid";
    param.order = "ASC";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "1");
    EXPECT_EQ(data.at(data.size() - 1).name, "a");

    param.order = "asc";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "a");
    EXPECT_EQ(data.at(data.size() - 1).name, "1");
}

TEST_F(AdvisorProcessUtilTest, SortFlowLocationDataTestOrderByTid) {
    Dic::Protocol::KernelDetailsParams param{};
    param.orderBy = "tid";
    param.order = "ASC";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "A");
    EXPECT_EQ(data.at(data.size() - 1).name, "a");

    param.order = "asc";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "a");
    EXPECT_EQ(data.at(data.size() - 1).name, "A");
}

TEST_F(AdvisorProcessUtilTest, SortFlowLocationDataTestOrderByStartTime) {
    Dic::Protocol::KernelDetailsParams param{};
    param.orderBy = "startTime";
    param.order = "ASC";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "1");
    EXPECT_EQ(data.at(data.size() - 1).name, "a");

    param.order = "asc";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "a");
    EXPECT_EQ(data.at(data.size() - 1).name, "1");
}

TEST_F(AdvisorProcessUtilTest, SortFlowLocationDataTestOrderByDefault) {
    Dic::Protocol::KernelDetailsParams param{};
    param.orderBy = "xxx";
    param.order = "ASC";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "1");
    EXPECT_EQ(data.at(data.size() - 1).name, "a");

    param.order = "asc";
    AdvisorProcessUtil::SortFlowLocationData(data, param);
    EXPECT_EQ(data.at(0).name, "1");
    EXPECT_EQ(data.at(data.size() - 1).name, "a");
}
