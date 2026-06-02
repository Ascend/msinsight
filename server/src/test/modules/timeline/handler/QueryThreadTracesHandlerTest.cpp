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
#include "QueryThreadTracesHandler.h"
#include "DomainObject.h"
#include "RenderEngine.h"
#include "TrackInfoManager.h"
#include "HandlerTest.cpp"

class QueryThreadTracesHandlerTest : HandlerTest {};

TEST_F(HandlerTest, QueryThreadTracesHandlerTestNormal) {
    Dic::Module::Timeline::QueryThreadTracesHandler handler;
    std::unique_ptr<Dic::Protocol::Request> requestPtr = std::make_unique<Dic::Protocol::UnitThreadTracesRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(HandlerTest, QueryTracesByTrackIdsTestNormal) {
    class MockRender : public Dic::Module::Timeline::RenderEngineInterface {
      public:
        ~MockRender() override = default;
        void SetDataEngineInterface(std::shared_ptr<Dic::Module::Timeline::DataEngineInterface>){};
        void QueryThreadTraces(const Protocol::UnitThreadTracesParams &requestParams,
            Protocol::UnitThreadTracesBody &responseBody, uint64_t minTimestamp, uint64_t traceId) {
            std::vector<ThreadTraces> temp;
            ThreadTraces temp1;
            temp1.startTime = 100; // 100;
            temp1.endTime = 200; // 200;
            temp1.id = "2";
            ThreadTraces temp2;
            temp2.startTime = 90; // 90;
            temp2.endTime = 170; // 170;
            temp2.id = "3";
            temp.emplace_back(temp1);
            temp.emplace_back(temp2);
            responseBody.data.emplace_back(temp);
        };
        bool QueryFlowCategoryEvents(Protocol::FlowCategoryEventsParams &params, uint64_t minTimestamp,
            std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> &flowDetailList) {
            return false;
        };
        void QueryThreadDetail(const Protocol::ThreadDetailParams &requestParams,
            Protocol::UnitThreadDetailBody &responseBody, uint64_t trackId) {};
        Dic::Module::Timeline::CompeteSliceDomain FindSliceByTimePoint(
            const std::string &fileId, const std::string &name, uint64_t timePoint, const std::string &metaType) {
            Dic::Module::Timeline::CompeteSliceDomain temp;
            return temp;
        };
    };
    Dic::Module::Timeline::TrackInfoManager::Instance().Reset();
    Dic::Module::Timeline::QueryThreadTracesHandler handler;
    std::shared_ptr<MockRender> mockRender = std::make_shared<MockRender>();
    handler.SetRenderEngine(mockRender);
    UnitThreadTracesRequest request;
    request.params.threadIdList.emplace_back("lllllll");
    UnitThreadTracesResponse response;
    handler.QueryTracesByTrackIds(request, response, 0);
    EXPECT_EQ(response.body.data.size(), 1); // 1;
    EXPECT_EQ(response.body.data[0].size(), 2); // 2;
    EXPECT_EQ(response.body.data[0][0].endTime, 170); // 170
    Dic::Module::Timeline::TrackInfoManager::Instance().Reset();
}
