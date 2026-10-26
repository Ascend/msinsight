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
#include <unordered_set>
#include "TimelineModule.h"
#include "RenderEngineInterface.h"
#include "DataBaseManager.h"
#include "RepositoryFactoryInterface.h"
#include "DataEngineInterface.h"
#define private public
#include "DataEngine.h"
#include "RenderEngine.h"
#undef private
#define protected public
#include "TimelineRequestHandler.h"
#undef protected
using namespace Dic;
using namespace Dic::Module;
using namespace Dic::Module::Timeline;
namespace {
class TimelineModuleTest : public ::testing::Test {
  protected:
    std::shared_ptr<DataEngineInterface> originalDataEngine;
    std::shared_ptr<RepositoryFactoryInterface> originalRepositoryFactory;
    void SetUp() override {
        originalDataEngine = RenderEngine::Instance()->dataEngine;
        originalRepositoryFactory = DataEngine::Instance()->respotoryFactory;
    }
    void TearDown() override {
        RenderEngine::Instance()->SetDataEngineInterface(originalDataEngine);
        DataEngine::Instance()->SetRepositoryFactory(originalRepositoryFactory);
    }
};
class InspectableTimelineModule : public TimelineModule {
  public:
    using BaseModule::requestHandlerMap;
    using BaseModule::moduleName;
};
class RecordingTimelineHandler : public ModuleRequestHandler {
  public:
    unsigned calls = 0;
    unsigned receivedId = 0;
    std::string receivedCommand;
    RecordingTimelineHandler() { async = false; }
    bool HandleRequest(std::unique_ptr<Dic::Protocol::Request> request) override {
        ++calls;
        receivedId = request->id;
        receivedCommand = request->command;
        return true;
    }
};
TEST_F(TimelineModuleTest, RegistersEverySupportedCommandAndReplacesOldHandlers) {
    InspectableTimelineModule module;
    EXPECT_EQ(module.moduleName, MODULE_TIMELINE);
    const std::vector<std::string> expected = {REQ_RES_UNIT_THREAD_TRACES, REQ_RES_UNIT_THREAD_TRACES_SUMMARY,
        REQ_RES_UNIT_THREADS, REQ_RES_UNIT_THREAD_DETAIL, REQ_RES_UNIT_FLOWS, REQ_RES_RESET_WINDOW,
        REQ_RES_UNIT_SET_CARD_ALIAS, REQ_RES_IMPORT_ACTION, REQ_RES_PARSE_CARDS, REQ_RES_SEARCH_COUNT,
        REQ_RES_SEARCH_SLICE, REQ_RES_REMOTE_DELETE, REQ_RES_FLOW_CATEGORY_LIST, REQ_RES_FLOW_CATEGORY_EVENTS,
        REQ_RES_UNIT_COUNTER, REQ_RES_UNIT_SYSTEM_VIEW, REQ_RES_UNIT_SYSTEM_VIEW_TRACE, REQ_RES_UNIT_KERNEL_DETAILS,
        REQ_RES_KERNEL_E2E_TIME, REQ_RES_FREE_ATTRIBUTION_SUMMARY, REQ_RES_FREE_ATTRIBUTION_DETAILS,
        REQ_RES_ONE_KERNEL_DETAILS, REQ_RES_TABLE_DATA_NAME_LIST, REQ_RES_TABLE_DATA_DETAIL,
        REQ_RES_COMMUNICATION_KERNEL_DETAIL, REQ_RES_SAME_OPERATORS_DURATION, REQ_RES_SEARCH_ALL_SLICES,
        REQ_RES_UNIT_EVENTS_VIEW, REQ_RES_SYSTEM_VIEW_OVERALL, REQ_RES_SYSTEM_VIEW_OVERALL_MORE_DETAILS,
        REQ_RES_EXPERT_ANALYSIS_AICORE_FREQ, REQ_RES_CREATE_CURVE, REQ_RES_MEMCPY_OVERALL, REQ_RES_KERNEL_OVERALL,
        REQ_RES_MEMCPY_DETAIL, REQ_RES_SYSTEM_VIEW_FTRACE_STAT, REQ_RES_SYSTEM_VIEW_KERNEL_MFU_LIST,
        REQ_RES_RANK_OFFSET};
    for (int pass = 0; pass < 2; ++pass) {
        module.RegisterRequestHandlers();
        ASSERT_EQ(module.requestHandlerMap.size(), expected.size());
        for (const auto &command : expected) {
            ASSERT_EQ(module.requestHandlerMap.count(command), 1U) << command;
            EXPECT_NE(module.requestHandlerMap.at(command), nullptr) << command;
        }
        module.requestHandlerMap.emplace("stale", std::make_unique<RecordingTimelineHandler>());
    }
}
TEST_F(TimelineModuleTest, InjectsSharedRendererIntoHandlersThatRequireIt) {
    InspectableTimelineModule module;
    module.RegisterRequestHandlers();
    for (const auto &command : {REQ_RES_UNIT_THREAD_TRACES, REQ_RES_UNIT_THREAD_DETAIL, REQ_RES_FLOW_CATEGORY_EVENTS}) {
        auto *handler = dynamic_cast<TimelineRequestHandler *>(module.requestHandlerMap.at(command).get());
        ASSERT_NE(handler, nullptr);
        EXPECT_EQ(handler->renderEngine, RenderEngine::Instance());
    }
}
TEST_F(TimelineModuleTest, DispatchesRequestPayloadAndUnknownCommandDoesNotInvokeHandler) {
    InspectableTimelineModule module;
    auto handler = std::make_unique<RecordingTimelineHandler>();
    auto *record = handler.get();
    module.requestHandlerMap.emplace("test-command", std::move(handler));
    auto request = std::make_unique<Dic::Protocol::Request>(std::string("test-command"));
    request->id = 70001;
    request->command = "test-command";
    module.OnRequest(std::move(request));
    EXPECT_EQ(record->calls, 1U);
    EXPECT_EQ(record->receivedId, 70001U);
    EXPECT_EQ(record->receivedCommand, "test-command");
    auto unknown = std::make_unique<Dic::Protocol::Request>(std::string("unknown"));
    unknown->id = 70002;
    unknown->command = "unknown";
    module.OnRequest(std::move(unknown));
    EXPECT_EQ(record->calls, 1U);
    EXPECT_EQ(module.requestHandlerMap.size(), 1U);
}
}
