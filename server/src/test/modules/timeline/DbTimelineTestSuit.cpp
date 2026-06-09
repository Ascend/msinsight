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

#include "../../FullDbTestSuit.cpp"
#include "TimelineTestUtil.h"
#include "RenderEngine.h"
#include "DataEngine.h"
#include "TrackInfoManager.h"
#include "RepositoryFactory.h"

class DbTimelineTestSuit : FullDbTestSuit {};

TEST_F(FullDbTestSuit, FullDb_of_SearchSliceNameCountWithFuzzyMatch) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    uint32_t expectCount = 1;
    Dic::Protocol::SearchCountParams params;
    params.searchContent = "hcom";
    params.rankId = "2"; // cardId = 2

    auto count = database->SearchSliceNameCount(params, {});
    EXPECT_EQ(count, expectCount);
}

TEST_F(FullDbTestSuit, FullDb_of_SearchSliceNameCountWithCaseMatch) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    uint32_t expectCount = 0;
    Dic::Protocol::SearchCountParams params;
    params.searchContent = "Hcom";
    params.rankId = "2"; // cardId = 2
    params.isMatchCase = true;

    auto count = database->SearchSliceNameCount(params, {});
    EXPECT_EQ(count, expectCount);
}

TEST_F(FullDbTestSuit, FullDb_of_SearchSliceNameCountWithExactMatch) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    uint32_t expectCount = 3;
    Dic::Protocol::SearchCountParams params;
    params.searchContent = "aclnnInplaceadd";
    params.rankId = "2"; // cardId = 2
    params.isMatchExact = true;

    auto count = database->SearchSliceNameCount(params, {});
    EXPECT_EQ(count, expectCount);
}

TEST_F(FullDbTestSuit, FullDb_of_SearchSliceNameCountWithCaseAndExactMatch) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    uint32_t expectCount = 0;
    Dic::Protocol::SearchCountParams params;
    params.searchContent = "aclnnInplaceadd";
    params.rankId = "2"; // cardId = 2
    params.isMatchExact = true;
    params.isMatchCase = true;

    auto count = database->SearchSliceNameCount(params, {});
    EXPECT_EQ(count, expectCount);
}

TEST_F(FullDbTestSuit, FullDb_of_SearchSliceName) {
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    Dic::Protocol::SearchSliceParams params;
    params.searchContent = "hcom";
    int index = 0;
    Dic::Protocol::SearchSliceBody body;

    body.rankId = "2"; // cardId = 2
    std::string expectPid = "Ascend Hardware";
    std::string expectTid = "8";
    uint64_t expectStartTime = 181306181;
    uint32_t expectDepth = 0;
    uint64_t expectDuration = 51121;

    database->SearchSliceName(params, index, minTimestamp, body, {});
    EXPECT_EQ(body.pid, expectPid);
    EXPECT_EQ(body.tid, expectTid);
    EXPECT_EQ(body.startTime, expectStartTime);
    EXPECT_EQ(body.depth, expectDepth);
    EXPECT_EQ(body.duration, expectDuration);
}

TEST_F(FullDbTestSuit, FullDb_of_QueryRankIds) {
    auto database = std::dynamic_pointer_cast<DbTraceDataBase, Timeline::VirtualTraceDatabase>(
        Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb"));
    EXPECT_NE(database, nullptr);
    DataBaseManager::Instance().SetFileType(FileType::MS_PROF, database->GetDbPath());
    auto rankIds = database->QueryRankId();
    EXPECT_EQ(rankIds.size(), 8); // size = 8
}

TEST_F(FullDbTestSuit, FullDb_of_ThreadTracesSummary) {
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    Dic::Protocol::UnitThreadTracesSummaryBody body;

    Dic::Protocol::UnitThreadTracesSummaryParams params;
    params.startTime = 0;
    params.cardId = "2";
    params.processId = "Ascend Hardware";
    params.metaType = "Ascend Hardware";
    params.endTime = 400000000; // endTime = 400000000

    database->QueryThreadTracesSummary(params, body, minTimestamp);
    const uint64_t expectSize1 = 24;
    EXPECT_EQ(body.data.size(), expectSize1); // size = 24
    EXPECT_EQ(body.data[0].startTime, 39828694); // startTime = 39828694

    body.data.clear();
    params.metaType = "HCCL";

    database->QueryThreadTracesSummary(params, body, minTimestamp);
    const uint64_t expectSize2 = 8;
    EXPECT_EQ(body.data.size(), expectSize2); // size = 8
    EXPECT_EQ(body.data[0].startTime, 175902293); // startTime = 175902293

    body.data.clear();
    params.metaType = "CANN_API";
    params.processId = "11814731181473";

    database->QueryThreadTracesSummary(params, body, minTimestamp);
    const uint64_t expectSize3 = 1;
    EXPECT_EQ(body.data.size(), expectSize3); // size = 3
    EXPECT_EQ(body.data[0].startTime, 39360870); // startTime = 39360870

    body.data.clear();
    params.metaType = "OVERLAP_ANALYSIS";

    database->QueryThreadTracesSummary(params, body, minTimestamp);
    const uint64_t expectSize4 = 1;
    EXPECT_EQ(body.data.size(), expectSize4);

    body.data.clear();
    params.metaType = "PROCESS";

    database->QueryThreadTracesSummary(params, body, minTimestamp);
    const uint64_t expectSize5 = 1;
    EXPECT_EQ(body.data.size(), expectSize5);

    body.data.clear();
    params.unitType = "label";
    params.metaType = "CANN_API";

    database->QueryThreadTracesSummary(params, body, minTimestamp);
    const uint64_t expectSize6 = 1;
    EXPECT_EQ(body.data.size(), expectSize6);

    params.metaType = "HBM";
    EXPECT_EQ(database->QueryThreadTracesSummary(params, body, minTimestamp), false);
}

TEST_F(FullDbTestSuit, FullDb_of_ThreadTracesSummary_MSTX) {
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    Dic::Protocol::UnitThreadTracesSummaryBody body;

    Dic::Protocol::UnitThreadTracesSummaryParams params;
    params.startTime = 0;
    params.cardId = "2";
    params.processId = "4398785246508220";
    params.metaType = "MSTX_EVENTS";
    params.endTime = 400000000; // endTime = 400000000

    database->QueryThreadTracesSummary(params, body, minTimestamp);
    const uint64_t expectSize = 0;
    EXPECT_EQ(body.data.size(), expectSize);

    body.data.clear();
}

TEST_F(FullDbTestSuit, FullDb_of_ThreadTracesList) {
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    Dic::Protocol::UnitThreadsBody body;

    Dic::Protocol::UnitThreadsParams params;
    params.startTime = 0;
    params.endTime = 400000000; // endTime = 400000000
    params.rankId = "2";
    Dic::Protocol::Metadata metadata{.tid = "8", .pid = "Ascend Hardware", .metaType = "Ascend Hardware"};
    params.metadataList.emplace_back(metadata);
    database->QueryThreads(params, body, minTimestamp, {0});
    EXPECT_EQ(body.data.size(), 1);
    EXPECT_EQ(body.data[0].title, "hcom_allReduce__305_880_1");
    EXPECT_EQ(body.data[0].selfTime, 51121); // selfTime = 51121
    EXPECT_EQ(body.data[0].occurrences, 1);

    body.data.clear();
    params.metadataList[0].tid = "1315_1";
    params.metadataList[0].metaType = "HCCL";
    database->QueryThreads(params, body, minTimestamp, {0});
    EXPECT_EQ(body.data.size(), 1);
    EXPECT_EQ(body.data[0].title, "Memcpy");
    EXPECT_EQ(body.data[0].selfTime, 1520); // selfTime = 1520
    EXPECT_EQ(body.data[0].occurrences, 1);

    body.data.clear();
    params.metadataList[0].metaType = "CANN_API";
    params.metadataList[0].tid = "20000";
    params.metadataList[0].pid = "11814731181473";

    database->QueryThreads(params, body, minTimestamp, {0});
    EXPECT_EQ(body.data.size(), 3); // size = 3
    EXPECT_EQ(body.data[0].title, "aclrtGetDeviceCount");
    EXPECT_EQ(body.data[0].selfTime, 2560); // selfTime = 2560
    EXPECT_EQ(body.data[0].occurrences, 1);

    params.metadataList[0].metaType = "HBM";
    EXPECT_EQ(database->QueryThreads(params, body, minTimestamp, {0}), true);
}

TEST_F(FullDbTestSuit, FullDb_of_UnitMetaData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    auto metaData = std::vector<std::unique_ptr<Protocol::UnitTrack>>();
    database->QueryUnitsMetadata("2", metaData);
    bool hasTopLevelCounterMetric = false;
    const Protocol::UnitTrack *npuMetrics = nullptr;
    for (const auto &item : metaData) {
        if (item->metaData.processName == "HBM" || item->metaData.processName == "LLC" ||
            item->metaData.processName == "DDR" || item->metaData.processName == "NPU MEM") {
            hasTopLevelCounterMetric = true;
        }
        if (item->metaData.processId == "__npu_metrics__") {
            npuMetrics = item.get();
        }
    }
    ASSERT_NE(npuMetrics, nullptr);
    EXPECT_EQ(npuMetrics->type, "label");
    bool hasHbmMetric = false;
    bool hasLlcMetric = false;
    bool hasDdrMetric = false;
    for (const auto &child : npuMetrics->children) {
        ASSERT_NE(child, nullptr);
        EXPECT_EQ(child->type, "label");
        hasHbmMetric = hasHbmMetric || child->metaData.processName == "HBM";
        hasLlcMetric = hasLlcMetric || child->metaData.processName == "LLC";
        hasDdrMetric = hasDdrMetric || child->metaData.processName == "DDR";
    }
    EXPECT_EQ(hasTopLevelCounterMetric, false);
    EXPECT_EQ(hasHbmMetric, true);
    EXPECT_EQ(hasLlcMetric, true);
    EXPECT_EQ(hasDdrMetric, true);
}

TEST_F(FullDbTestSuit, FullDb_of_HostMetaData) {
    auto database = std::dynamic_pointer_cast<DbTraceDataBase, Timeline::VirtualTraceDatabase>(
        Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb"));

    auto metaData = std::vector<std::unique_ptr<Protocol::UnitTrack>>();
    database->QueryHostMetadata("2", metaData);

    EXPECT_EQ(metaData.size(), 2); // size = 2
    EXPECT_EQ(metaData[0]->children.size(), 1);
}

TEST_F(FullDbTestSuit, FullDb_of_QueryKernelDetailData) {
    Dic::Protocol::KernelDetailsParams requestParams;
    requestParams.current = 1;
    requestParams.pageSize = 20; // pageSize = 20
    requestParams.order = "ASC";
    requestParams.orderBy = "name";
    requestParams.coreType = "AI_VECTOR_CORE";
    requestParams.searchName = "aclnn";
    requestParams.rankId = "2";
    requestParams.deviceId = "2";
    Dic::Protocol::KernelDetailsBody responseBody;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = std::dynamic_pointer_cast<DbTraceDataBase, Timeline::VirtualTraceDatabase>(
        Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb"));

    database->QueryKernelDetailData(requestParams, responseBody, minTimestamp);

    EXPECT_EQ(responseBody.kernelDetails.size(), 11); // size = 11
    EXPECT_EQ(responseBody.acceleratorCoreList.size(), 1);
}

TEST_F(FullDbTestSuit, FullDb_of_UnitCounter) {
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    Dic::Protocol::UnitCounterParams params;
    params.startTime = 0;
    params.rankId = "2";
    params.pid = "HBM";
    params.metaType = "HBM";
    params.threadName = "HBM 1 read/Bandwidth";
    params.threadId = "HBM 1 read/Bandwidth";
    params.endTime = 400000000; // endTime = 400000000

    auto counterData = std::vector<Protocol::UnitCounterData>();
    database->QueryUnitCounter(params, minTimestamp, counterData);
    EXPECT_EQ(counterData.size(), 2);

    params.pid = "LLC";
    params.metaType = "LLC";
    params.threadName = "LLC 0 read/Hit Rate";
    params.threadId = "LLC 0 read/Hit Rate";
    counterData.clear();
    database->QueryUnitCounter(params, minTimestamp, counterData);
    EXPECT_EQ(counterData.size(), 4); // size = 4

    params.pid = "DDR";
    params.metaType = "DDR";
    params.threadName = "Read";
    params.threadId = "Read";
    counterData.clear();
    database->QueryUnitCounter(params, minTimestamp, counterData);
    EXPECT_EQ(counterData.size(), 6); // size = 6

    params.metaType = "Ascend Hardware";
    EXPECT_EQ(database->QueryUnitCounter(params, minTimestamp, counterData), false);
}

TEST_F(FullDbTestSuit, QueryEventsViewData4HostProcess) {
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    Dic::Protocol::EventsViewParams params;
    params.currentPage = CUR_PAGE_1;
    params.metaType = "PROCESS";
    params.order = "descend";
    params.orderBy = "duration";
    params.pageSize = SYS_PAGE_SIZE;
    params.pid = "11814731181473";
    params.processName = "Process 2750";
    params.rankId = "2";

    Dic::Protocol::EventsViewBody body;
    database->QueryEventsViewData(params, body, minTimestamp);
    const uint64_t EXPECT_COUNT = 4;
    const uint64_t EXPECT_START = 39360870;
    const uint64_t EXPECT_DURATION = 2560;
    const uint64_t EXPECT_DEPTH = 0;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Api(body);
    EXPECT_EQ(body.eventDetailList.size(), EXPECT_COUNT);

    auto ptr = dynamic_cast<Dic::Protocol::HostEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "aclrtGetDeviceCount");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->tid, "3571117473");
    EXPECT_EQ(ptr->pid, "2750");
    EXPECT_EQ(ptr->processId, "11814731181473");
    EXPECT_EQ(ptr->threadId, "20000");
    EXPECT_EQ(ptr->depth, EXPECT_DEPTH);
}

TEST_F(FullDbTestSuit, QueryEventsViewData4HostProcessThread) {
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    Dic::Protocol::EventsViewParams params;
    params.currentPage = CUR_PAGE_1;
    params.metaType = "CANN_API";
    params.order = "descend";
    params.orderBy = "duration";
    params.pageSize = SYS_PAGE_SIZE;
    params.pid = "11814731181473";
    params.processName = "Thread 3571117473";
    params.rankId = "2";

    Dic::Protocol::EventsViewBody body;
    database->QueryEventsViewData(params, body, minTimestamp);
    const uint64_t EXPECT_COUNT = 4;
    const uint64_t EXPECT_START = 39360870;
    const uint64_t EXPECT_DURATION = 2560;
    const uint64_t EXPECT_DEPTH = 0;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Api(body);
    EXPECT_EQ(body.eventDetailList.size(), EXPECT_COUNT);

    auto ptr = dynamic_cast<Dic::Protocol::HostEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "aclrtGetDeviceCount");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->tid, "3571117473");
    EXPECT_EQ(ptr->pid, "2750");
    EXPECT_EQ(ptr->processId, "11814731181473");
    EXPECT_EQ(ptr->threadId, "20000");
    EXPECT_EQ(ptr->depth, EXPECT_DEPTH);
}

TEST_F(FullDbTestSuit, QueryEventsViewData4HostProcessThreadCANN) {
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    Dic::Protocol::EventsViewParams params;
    params.currentPage = CUR_PAGE_1;
    params.metaType = "CANN_API";
    params.order = "descend";
    params.orderBy = "duration";
    params.pageSize = SYS_PAGE_SIZE;
    params.pid = "11814731181473";
    params.processName = "CANN";
    params.rankId = "2";

    Dic::Protocol::EventsViewBody body;
    database->QueryEventsViewData(params, body, minTimestamp);
    const uint64_t EXPECT_COUNT = 4;
    const uint64_t EXPECT_START = 39360870;
    const uint64_t EXPECT_DURATION = 2560;
    const uint64_t EXPECT_DEPTH = 0;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Api(body);
    EXPECT_EQ(body.eventDetailList.size(), EXPECT_COUNT);

    auto ptr = dynamic_cast<Dic::Protocol::HostEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "aclrtGetDeviceCount");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->tid, "3571117473");
    EXPECT_EQ(ptr->pid, "2750");
    EXPECT_EQ(ptr->processId, "11814731181473");
    EXPECT_EQ(ptr->threadId, "20000");
    EXPECT_EQ(ptr->depth, EXPECT_DEPTH);
}

TEST_F(FullDbTestSuit, QueryEventsViewData4HostProcessThreadCANNAcl) {
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");
    Dic::Protocol::EventsViewParams params;
    params.currentPage = CUR_PAGE_1;
    params.metaType = "CANN_API";
    params.order = "descend";
    params.orderBy = "duration";
    params.pageSize = SYS_PAGE_SIZE;
    params.pid = "11814731181473";
    params.processName = "CANN";
    params.rankId = "2";
    params.threadName = "acl";
    params.tid = "20000";

    Dic::Protocol::EventsViewBody body;
    database->QueryEventsViewData(params, body, minTimestamp);
    const uint64_t EXPECT_COUNT = 4;
    const uint64_t EXPECT_START = 39360870;
    const uint64_t EXPECT_DURATION = 2560;
    const uint64_t EXPECT_DEPTH = 0;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Api(body);
    EXPECT_EQ(body.eventDetailList.size(), EXPECT_COUNT);

    auto ptr = dynamic_cast<Dic::Protocol::HostEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "aclrtGetDeviceCount");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->tid, "3571117473");
    EXPECT_EQ(ptr->pid, "2750");
    EXPECT_EQ(ptr->processId, "11814731181473");
    EXPECT_EQ(ptr->threadId, "20000");
    EXPECT_EQ(ptr->depth, EXPECT_DEPTH);
}

TEST_F(FullDbTestSuit, QueryComputeStatisticsData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    Protocol::SummaryStatisticParams requestParams;
    Protocol::SummaryStatisticsBody responseBody;

    database->QueryComputeStatisticsData(requestParams, responseBody);
    const uint64_t EXPECT_COUNT = 1;

    EXPECT_EQ(responseBody.summaryStatisticsItemList.size(), EXPECT_COUNT);
}

TEST_F(FullDbTestSuit, QueryInsightHostToDeviceFlowCategoryEvents) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    Protocol::FlowCategoryEventsParams params;
    params.rankId = "Fulldb";
    params.category = "HostToDevice";
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> flowDetailList;
    renderEngine->QueryFlowCategoryEvents(params, minTimestamp, flowDetailList);
    const uint64_t EXPECT_COUNT = 0;
    EXPECT_EQ(flowDetailList.size(), EXPECT_COUNT);
}

TEST_F(FullDbTestSuit, QueryInsightMsTxFlowCategoryEvents) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    Protocol::FlowCategoryEventsParams params;
    params.rankId = "Fulldb";
    params.category = "MsTx";
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> flowDetailList;
    renderEngine->QueryFlowCategoryEvents(params, minTimestamp, flowDetailList);
    const uint64_t EXPECT_COUNT = 0;
    EXPECT_EQ(flowDetailList.size(), EXPECT_COUNT);
}

TEST_F(FullDbTestSuit, QueryInsightAsyncTaskQueueFlowCategoryEvents) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    Protocol::FlowCategoryEventsParams params;
    params.rankId = "Fulldb";
    params.category = "async_task_queue";
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> flowDetailList;
    renderEngine->QueryFlowCategoryEvents(params, minTimestamp, flowDetailList);
    const uint64_t EXPECT_COUNT = 0;
    EXPECT_EQ(flowDetailList.size(), EXPECT_COUNT);
}

TEST_F(FullDbTestSuit, QueryInsightFwdbwdFlowCategoryEvents) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    Protocol::FlowCategoryEventsParams params;
    params.rankId = "Fulldb";
    params.category = "fwdbwd";
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> flowDetailList;
    renderEngine->QueryFlowCategoryEvents(params, minTimestamp, flowDetailList);
    const uint64_t EXPECT_COUNT = 0;
    EXPECT_EQ(flowDetailList.size(), EXPECT_COUNT);
}

TEST_F(FullDbTestSuit, QueryInsightAsyncNpuFlowCategoryEvents) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    Protocol::FlowCategoryEventsParams params;
    params.rankId = "Fulldb";
    params.category = "async_npu";
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> flowDetailList;
    renderEngine->QueryFlowCategoryEvents(params, minTimestamp, flowDetailList);
    const uint64_t EXPECT_COUNT = 0;
    EXPECT_EQ(flowDetailList.size(), EXPECT_COUNT);
}

TEST_F(FullDbTestSuit, QueryCANNSliceByNameListSuccess) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    std::vector<CompeteSliceDomain> res =
        renderEngine->QuerySliceDetailByNameList("FullDb", DataType::DB, "CANN", {"aclrtGetDeviceCount"});
    const int expectedSize = 1;
    EXPECT_EQ(res.size(), expectedSize);
}

TEST_F(FullDbTestSuit, QueryHardwareSliceByNameListSuccess) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    std::vector<CompeteSliceDomain> res = renderEngine->QuerySliceDetailByNameList(
        "FullDb", DataType::DB, "Ascend Hardware", {"aclnnInplaceZero_ZerosLikeAiCore_ZerosLike"});
    const int expectedSize = 1;
    EXPECT_EQ(res.size(), expectedSize);
}

TEST_F(FullDbTestSuit, QueryUnitFLows) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    Protocol::UnitFlowsParams requestParams;
    requestParams.rankId = "2";
    requestParams.metaType = "Ascend Hardware";
    requestParams.id = "0";
    Protocol::UnitFlowsBody responseBody;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();

    database->QueryUnitFlows(requestParams, responseBody, minTimestamp, 0);
    const uint64_t EXPECT_COUNT = 1;

    EXPECT_EQ(responseBody.unitAllFlows.size(), EXPECT_COUNT);
}

TEST_F(FullDbTestSuit, QueryTotalKernel) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    Protocol::KernelDetailsParams requestParams;
    requestParams.coreType = "";
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto result = database->QueryTotalKernel(requestParams, minTimestamp);
    const uint64_t EXPECT_COUNT = 11;

    EXPECT_EQ(result, EXPECT_COUNT);
}

TEST_F(FullDbTestSuit, QueryKernelDepthAndThread) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    Protocol::KernelParams params;
    params.name = "aclnnInplaceZero_ZerosLikeAiCore_ZerosLike";
    params.timestamp = 39828694; // timestamp = 39828694
    Protocol::OneKernelBody responseBody;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();

    database->QueryKernelDepthAndThread(params, responseBody, minTimestamp);
    const std::string EXPECT_ID = "0";

    EXPECT_EQ(responseBody.id, EXPECT_ID);
}

TEST_F(FullDbTestSuit, QueryCommunicationKernelInfoDbSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    Protocol::KernelParams params;
    Dic::Protocol::CommunicationKernelBody responseBody;

    database->QueryCommunicationKernelInfo("aclnnInplaceZero_ZerosLikeAiCore_ZerosLike", "FullDb", responseBody);
    EXPECT_EQ(responseBody.id, "");
}

TEST_F(FullDbTestSuit, SearchAllSlicesDetails) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    Protocol::SearchAllSliceParams params;
    params.current = 1;
    params.pageSize = 100; // pageSize = 100
    params.searchContent = "aclnn";
    params.rankId = "2";
    params.orderBy = "name";
    Protocol::SearchAllSlicesBody body;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();

    database->SearchAllSlicesDetails(params, body, minTimestamp, {});
    const uint64_t EXPECT_COUNT = 23;

    EXPECT_EQ(body.searchAllSlices.size(), EXPECT_COUNT);
}

TEST_F(FullDbTestSuit, TestQueryThreadSameOperatorsDetailsWhenHccl) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    requestParams.processes.push_back(SimpleProcess{"HCCL", {"0"}});
    requestParams.rankId = "2";
    requestParams.metaTypeList = {"HCCL"};
    requestParams.orderBy = "duration";
    requestParams.order = "DESC";
    requestParams.current = 1;
    requestParams.pageSize = 10; // pageSize = 10
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    const std::vector<uint64_t> traceId = {0};
    bool result = database->QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceId);
    EXPECT_EQ(result, true);
}

TEST_F(FullDbTestSuit, TestQueryThreadSameOperatorsDetailsWhenOverlap) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("FullDb");

    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    requestParams.processes.push_back(SimpleProcess{"17738580008830245", {"0"}});
    requestParams.metaTypeList = {"OVERLAP_ANALYSIS"};
    requestParams.orderBy = "duration";
    requestParams.order = "DESC";
    requestParams.current = 1;
    requestParams.pageSize = 100; // expect page size 100
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    const std::vector<uint64_t> traceId = {0};
    bool result = database->QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceId);
    EXPECT_EQ(result, true);
}
