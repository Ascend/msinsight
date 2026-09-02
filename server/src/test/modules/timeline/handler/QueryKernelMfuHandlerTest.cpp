/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
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

#include "HandlerTest.cpp"
#include "QueryKernelMfuAvailabilityHandler.h"
#include "QueryKernelMfuListHandler.h"

class QueryKernelMfuHandlerTest : public HandlerTest {};

TEST_F(QueryKernelMfuHandlerTest, OptionalThreadingProbeReportsMissingClusterDatabaseAsUnavailable) {
    Dic::Module::Timeline::QueryKernelMfuAvailabilityHandler handler;
    auto request = std::make_unique<Dic::Protocol::KernelMfuAvailabilityRequest>();
    request->params.clusterPath = "threading-analysis-without-cluster-database";
    request->params.allowMissingDatabase = true;

    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
}

TEST_F(QueryKernelMfuHandlerTest, StandardAvailabilityProbePreservesMissingDatabaseError) {
    Dic::Module::Timeline::QueryKernelMfuAvailabilityHandler handler;
    auto request = std::make_unique<Dic::Protocol::KernelMfuAvailabilityRequest>();
    request->params.clusterPath = "standard-project-without-cluster-database";

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
}

TEST_F(QueryKernelMfuHandlerTest, StandardListRequestPreservesMissingDatabaseError) {
    Dic::Module::Timeline::QueryKernelMfuListHandler handler;
    auto request = std::make_unique<Dic::Protocol::KernelMfuListRequest>();
    request->params.clusterPath = "standard-project-without-cluster-database";
    request->params.current = 1;
    request->params.pageSize = 10;

    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
}
