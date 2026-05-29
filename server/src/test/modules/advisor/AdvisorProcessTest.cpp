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

#include "../../TestSuit.h"
#include "ProtocolDefs.h"
#include "AclnnOpAdvisor.h"
#include "AffinityAPIAdvisor.h"
#include "AffinityOptimizerAdvisor.h"
#include "AICpuOpAdvisor.h"
#include "FusedOpAdvisor.h"
#include "OperatorDispatchAdvisor.h"
#include "DataBaseManager.h"

using namespace Dic::Protocol;
using namespace Dic::Module::Advisor;
using namespace Dic::Module::Timeline;

class AdvisorProcessTest : public TestSuit {
  protected:
    APITypeParams params = {.rankId = "0",
        .deviceId = "0",
        .currentPage = 2,
        .pageSize = 20, // 20 record per page, and now page 2
        .orderBy = "duration",
        .orderType = "ascend"};
};

TEST_F(AdvisorProcessTest, AclnnOpAdvisorProcessSuccessfulWithSpecificParam) {
    AclnnOperatorResBody resBody = {};
    auto res = AclnnOpAdvisor::Process(params, resBody);
    EXPECT_EQ(res, true);
    EXPECT_EQ(resBody.size, 0);
}

TEST_F(AdvisorProcessTest, AffinityAPIAdvisorProcessSuccessfulWithSpecificParam) {
    AffinityAPIResBody resBody = {};
    auto res = AffinityAPIAdvisor::Process(params, resBody);
    EXPECT_EQ(res, true);
    EXPECT_EQ(resBody.size, 24); // There are 24 that meet the criteria
    EXPECT_EQ(resBody.data.size(), 4); // There are 4 that in page 2
}

TEST_F(AdvisorProcessTest, AffinityOptimizerAdvisorProcessSuccessfulWithSpecificParam) {
    AffinityOptimizerResBody resBody = {};
    auto res = AffinityOptimizerAdvisor::Process(params, resBody);
    EXPECT_EQ(res, true);
    EXPECT_EQ(resBody.size, 1);
}

TEST_F(AdvisorProcessTest, AICpuOpAdvisorProcessSuccessfulWithSpecificParam) {
    AICpuOperatorResBody resBody = {};
    auto res = AICpuOpAdvisor::Process(params, resBody);
    EXPECT_EQ(res, true);
    EXPECT_EQ(resBody.size, 2); // The size of resBody is 2
}

TEST_F(AdvisorProcessTest, FusedOpAdvisorProcessSuccessfulWithSpecificParam) {
    OperatorFusionResBody resBody = {};
    auto res = FusedOpAdvisor::Process(params, resBody);
    EXPECT_EQ(res, true);
    EXPECT_EQ(resBody.size, 0);
}

TEST_F(AdvisorProcessTest, OperatorDispatchAdvisorProcessSuccessfulWithSpecificParam) {
    OperatorDispatchResBody resBody = {};
    auto res = OperatorDispatchAdvisor::Process(params, resBody);
    EXPECT_EQ(res, true);
    EXPECT_EQ(resBody.size, 529); // The size of resBody is 529
}
