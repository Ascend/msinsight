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
#include <algorithm>
#include "TimelineProtocolRequest.h"
#include "DataBaseManager.h"
#include "../../TestSuit.h"
#include "cmath"
#include "TimelineTestUtil.h"
#include "TraceDatabaseHelper.h"
#include "RenderEngine.h"
#include "DataEngine.h"
#include "TrackInfoManager.h"
#include "RepositoryFactory.h"

class TimelineTest : TestSuit {};

TEST_F(TestSuit, QuerySystemViewData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    Dic::Protocol::SystemViewBody responseBody;
    uint64_t PAGE = 10;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.layer = "Python";
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "name";
    requestParams.pageSize = PAGE;
    const uint64_t minTimestamp = 0;
    database->QuerySystemViewData(requestParams, responseBody, minTimestamp);
    int expectSize = 10;
    EXPECT_EQ(responseBody.systemViewDetail.size(), expectSize);
}

TEST_F(TestSuit, QuerySystemViewTraceData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    Dic::Protocol::SystemViewTraceBody responseBody;
    uint64_t PAGE = 10;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.layer = "Python";
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "timestamp";
    requestParams.pageSize = PAGE;
    const uint64_t minTimestamp = 0;
    database->QuerySystemViewTraceData(requestParams, responseBody, minTimestamp);
    int expectSize = 10;
    EXPECT_EQ(responseBody.systemViewDetail.size(), expectSize);
    EXPECT_EQ(responseBody.pageSize, PAGE);
    EXPECT_EQ(responseBody.currentPage, 1);
}

TEST_F(TestSuit, QuerySystemViewTraceDataWithHCCL) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    Dic::Protocol::SystemViewTraceBody responseBody;
    uint64_t PAGE = 5;
    requestParams.rankId = "0";
    requestParams.deviceId = "24";
    requestParams.layer = "HCCL";
    requestParams.current = 1;
    requestParams.order = "ascend";
    requestParams.orderBy = "timestamp";
    requestParams.pageSize = PAGE;
    const uint64_t minTimestamp = 0;
    database->QuerySystemViewTraceData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(responseBody.pageSize, PAGE);
    EXPECT_EQ(responseBody.currentPage, 1);
    EXPECT_EQ(responseBody.systemViewDetail.size(), PAGE);
}

TEST_F(TestSuit, QuerySystemViewTraceDataWithCommunication) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    Dic::Protocol::SystemViewTraceBody responseBody;
    uint64_t PAGE = 5;
    requestParams.rankId = "0";
    requestParams.deviceId = "24";
    requestParams.layer = "COMMUNICATION";
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "duration";
    requestParams.pageSize = PAGE;
    const uint64_t minTimestamp = 0;
    database->QuerySystemViewTraceData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(responseBody.pageSize, PAGE);
    EXPECT_EQ(responseBody.currentPage, 1);
    EXPECT_EQ(responseBody.systemViewDetail.size(), PAGE);
}

TEST_F(TestSuit, QuerySystemViewTraceDataWithSearchName) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    Dic::Protocol::SystemViewTraceBody responseBody;
    uint64_t PAGE = 10;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.layer = "Python";
    requestParams.searchName = "aten";
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "timestamp";
    requestParams.pageSize = PAGE;
    const uint64_t minTimestamp = 0;
    database->QuerySystemViewTraceData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(responseBody.pageSize, PAGE);
    EXPECT_EQ(responseBody.currentPage, 1);
    for (const auto &detail : responseBody.systemViewDetail) {
        std::string lowerName = detail.name;
        std::transform(
            lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return std::tolower(c); });
        EXPECT_TRUE(lowerName.find("aten") != std::string::npos);
    }
}

TEST_F(TestSuit, QuerySystemViewTraceDataWithTimeRange) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    Dic::Protocol::SystemViewTraceBody responseBody;
    uint64_t PAGE = 10;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.layer = "Python";
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "timestamp";
    requestParams.pageSize = PAGE;
    requestParams.startTime = 100000;
    requestParams.endTime = 500000;
    const uint64_t minTimestamp = 0;
    database->QuerySystemViewTraceData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(responseBody.pageSize, PAGE);
    EXPECT_EQ(responseBody.currentPage, 1);
    for (const auto &detail : responseBody.systemViewDetail) {
        EXPECT_GE(detail.startTime, requestParams.startTime);
        EXPECT_LE(detail.startTime, requestParams.endTime);
    }
}

TEST_F(TestSuit, QuerySystemViewTraceDataWithPagination) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    Dic::Protocol::SystemViewTraceBody responseBody1;
    Dic::Protocol::SystemViewTraceBody responseBody2;
    uint64_t PAGE = 5;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.layer = "Python";
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "timestamp";
    requestParams.pageSize = PAGE;
    const uint64_t minTimestamp = 0;
    database->QuerySystemViewTraceData(requestParams, responseBody1, minTimestamp);
    requestParams.current = 2;
    database->QuerySystemViewTraceData(requestParams, responseBody2, minTimestamp);
    EXPECT_EQ(responseBody1.pageSize, PAGE);
    EXPECT_EQ(responseBody2.pageSize, PAGE);
    EXPECT_EQ(responseBody1.currentPage, 1);
    EXPECT_EQ(responseBody2.currentPage, 2);
    EXPECT_EQ(responseBody1.systemViewDetail.size(), PAGE);
}

TEST_F(TestSuit, QuerySystemViewAICoreFreqData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewAICoreFreqParams requestParams;
    Dic::Protocol::ExpAnaAICoreFreqBody responseBody;
    std::vector<std::pair<uint64_t, uint64_t>> freqs;
    uint64_t maxFreq = 0;
    uint64_t minFreq = UINT64_MAX;
    database->QueryExpAnaAICoreFreqData(requestParams, responseBody, freqs, maxFreq, minFreq);
    int expectSize = 0;
    EXPECT_EQ(freqs.size(), expectSize);
}

TEST_F(TestSuit, QueryPythonViewWithTotalNum) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    Dic::Protocol::SystemViewBody responseBody;
    uint64_t PAGE = 10;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.layer = "Python";
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "name";
    requestParams.pageSize = PAGE;
    requestParams.isQueryTotal = true;
    const uint64_t minTimestamp = 0;
    database->QuerySystemViewData(requestParams, responseBody, minTimestamp);
    int expectSize = 162;
    EXPECT_EQ(responseBody.total, expectSize);
}

TEST_F(TestSuit, QueryLayerOperatorTimeWithCANN) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.layer = "CANN";
    const uint64_t minTimestamp = 0;
    const Dic::Module::Timeline::LayerStatData &data = database->QueryLayerData(requestParams, "%%", minTimestamp, "");
    int expectSize = 158266100;
    EXPECT_EQ(lround(data.allOperatorTime), expectSize);
}

TEST_F(TestSuit, QueryLayerOperatorTimeWithPython) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.layer = "Python";
    const uint64_t minTimestamp = 0;
    const Dic::Module::Timeline::LayerStatData &data = database->QueryLayerData(requestParams, "%%", minTimestamp, "");
    int expectSize = 851869940;
    EXPECT_EQ(lround(data.allOperatorTime), expectSize);
}

TEST_F(TestSuit, QueryLayerOperatorTimeWithAscend) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "12";
    requestParams.layer = "Ascend Hardware";
    const uint64_t minTimestamp = 0;
    const Dic::Module::Timeline::LayerStatData &data = database->QueryLayerData(requestParams, "%%", minTimestamp, "");
    int expectSize = 842011262;
    EXPECT_EQ(lround(data.allOperatorTime), expectSize);
    EXPECT_EQ(data.total, 57); // total operator = 57
}

TEST_F(TestSuit, QueryLayerOperatorTimeWithHCCL) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "24";
    requestParams.layer = "HCCL";
    const uint64_t minTimestamp = 0;
    const Dic::Module::Timeline::LayerStatData &data = database->QueryLayerData(requestParams, "%%", minTimestamp, "");
    int expectSize = 449202040;
    EXPECT_EQ(lround(data.allOperatorTime), expectSize);
}

TEST_F(TestSuit, QueryLayerOperatorTimeWithCommunication) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "24";
    requestParams.layer = "COMMUNICATION";
    const uint64_t minTimestamp = 0;
    const Dic::Module::Timeline::LayerStatData &data = database->QueryLayerData(requestParams, "%%", minTimestamp, "");
    int expectSize = 449202040;
    EXPECT_EQ(lround(data.allOperatorTime), expectSize);
}

TEST_F(TestSuit, QueryLayerOperatorTimeWithOverlap) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "4";
    requestParams.layer = "Overlap Analysis";
    const uint64_t minTimestamp = 0;
    const Dic::Module::Timeline::LayerStatData &data = database->QueryLayerData(requestParams, "%%", minTimestamp, "");
    int expectSize = 292195068;
    EXPECT_EQ(lround(data.allOperatorTime), expectSize);
}

TEST_F(TestSuit, QueryCoreType) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");

    const std::vector<std::string> &coreType = database->QueryCoreType();
    int expectSize = 7;
    EXPECT_EQ(coreType.size(), expectSize);
}

TEST_F(TestSuit, QueryKernelDetailData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::KernelDetailsParams requestParams;
    Dic::Protocol::KernelDetailsBody responseBody;
    uint64_t PAGE = 10;
    requestParams.deviceId = "0";
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "name";
    requestParams.pageSize = PAGE;
    requestParams.rankId = "0";
    database->QueryKernelDetailData(requestParams, responseBody, 0);
    int expectSize = 20;
    EXPECT_EQ(responseBody.count, expectSize);
}

TEST_F(TestSuit, QueryTotalKernel) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::KernelDetailsParams requestParams;
    uint64_t PAGE = 10;
    requestParams.deviceId = "0";
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "name";
    requestParams.pageSize = PAGE;
    requestParams.rankId = "0";
    requestParams.filters.emplace_back("type", "hcom");
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto result = database->QueryTotalKernel(requestParams, minTimestamp);
    const uint64_t EXPECT_COUNT = 4;

    EXPECT_EQ(result, EXPECT_COUNT);
}

TEST_F(TestSuit, QueryKernelDetailDataWithCoreType) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::KernelDetailsParams requestParams;
    Dic::Protocol::KernelDetailsBody responseBody;
    uint64_t PAGE = 10;
    requestParams.current = 1;
    requestParams.order = "descend";
    requestParams.orderBy = "name";
    requestParams.pageSize = PAGE;
    requestParams.rankId = "0";
    requestParams.coreType = "AI_CORE";
    requestParams.searchName = "";
    requestParams.deviceId = "0";
    database->QueryKernelDetailData(requestParams, responseBody, 0);
    int expectSize = 0;
    EXPECT_EQ(responseBody.count, expectSize);
}

TEST_F(TestSuit, QueryKernelDepthAndThread) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::KernelParams requestParams;
    Dic::Protocol::OneKernelBody responseBody;
    uint64_t DURATION = 72861;
    uint64_t TIMESTAMP = 1695115378736217000;
    requestParams.duration = DURATION;
    requestParams.name = "trans_Cast_15";
    requestParams.timestamp = 0;
    uint64_t minTime = TIMESTAMP;
    database->QueryKernelDepthAndThread(requestParams, responseBody, minTime);
    uint64_t depth = 0;
    std::string tid = "17";
    std::string pid = "300";
    EXPECT_EQ(responseBody.depth, depth);
    EXPECT_EQ(responseBody.threadId, tid);
    EXPECT_EQ(responseBody.pid, pid);
}

TEST_F(TestSuit, QueryCommunicationKernelInfoSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::CommunicationKernelBody responseBody;
    database->QueryCommunicationKernelInfo("trans_Cast_15", "0", responseBody);
    uint64_t depth = 0;
    std::string tid = "17";
    std::string pid = "300";
    EXPECT_EQ(responseBody.depth, depth);
    EXPECT_EQ(responseBody.threadId, tid);
    EXPECT_EQ(responseBody.pid, pid);
}

TEST_F(TestSuit, QueryCommunicationStatisticsData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SummaryStatisticParams requestParams;
    Dic::Protocol::SummaryStatisticsBody responseBody;
    database->QueryCommunicationStatisticsData(requestParams, responseBody);
    EXPECT_EQ(responseBody.summaryStatisticsItemList.size(), 2); // item number = 2
}

TEST_F(TestSuit, QueryUnitCounterData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::UnitCounterParams requestParams;
    std::vector<Dic::Protocol::UnitCounterData> unitData;
    uint64_t STARTTIME = 1695115378653323500;
    uint64_t ENDTIME = 1695115378673307500;
    requestParams.pid = "14083661300";
    requestParams.threadName = "APP/DDR";
    requestParams.startTime = STARTTIME;
    requestParams.endTime = ENDTIME;
    database->QueryUnitCounter(requestParams, 0, unitData);
    EXPECT_EQ(unitData.size(), 2); // unit data size = 2
    EXPECT_EQ(unitData[0].timestamp, 1695115378653323500); // timestamp = 1695115378653323500
}

TEST_F(TestSuit, QueryFlowCategoryEvents) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    Dic::Protocol::FlowCategoryEventsParams requestParams;
    std::vector<std::unique_ptr<Dic::Protocol::UnitSingleFlow>> flowDetailList;
    uint64_t STARTTIME = 9171200;
    uint64_t ENDTIME = 20617000;
    requestParams.startTime = STARTTIME;
    requestParams.endTime = ENDTIME;
    requestParams.timePerPx = 1;
    requestParams.category = "HostToDevice";
    requestParams.rankId = "0";
    uint64_t minTimestamp = 1695115378704489800;
    const int expectSize = 12;
    const uint64_t expectFromTimestamp = 17654000;
    const uint64_t expectToTimestamp = 17902700;
    renderEngine->QueryFlowCategoryEvents(requestParams, minTimestamp, flowDetailList);
    std::string EXPECT_FROM_TID = "1408366";
    EXPECT_EQ(flowDetailList.size(), expectSize); // flowDetailList size = 5
    EXPECT_EQ(flowDetailList[0]->cat, "HostToDevice");
    EXPECT_EQ(flowDetailList[0]->from.timestamp, expectFromTimestamp); // timestamp = 1695115378722143800
    EXPECT_EQ(flowDetailList[0]->from.pid, "140836602");
    EXPECT_EQ(flowDetailList[0]->from.depth, 0);
    EXPECT_EQ(flowDetailList[0]->from.tid, EXPECT_FROM_TID); // from.tid = 1408366
    EXPECT_EQ(flowDetailList[0]->to.timestamp, expectToTimestamp); // to.timestamp = 1695115378722392500
    EXPECT_EQ(flowDetailList[0]->to.pid, "14083661400");
    EXPECT_EQ(flowDetailList[0]->to.depth, 0);
    EXPECT_EQ(flowDetailList[0]->to.tid, "0"); // to.tid = 0
}

TEST_F(TestSuit, QuerySliceDetailByNameListSuccess) {
    auto renderEngine = GetRenderEngine();
    std::vector<CompeteSliceDomain> res =
        renderEngine->QuerySliceDetailByNameList("0", DataType::TEXT, "CANN", {"AscendCL@aclrtRecordEvent"});
    const int expectedSize = 115;
    EXPECT_EQ(res.size(), expectedSize);
}

TEST_F(TestSuit, QueryFlowCategoryEventsWithLockRange) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    Dic::Protocol::FlowCategoryEventsParams requestParams;
    std::vector<std::unique_ptr<Dic::Protocol::UnitSingleFlow>> flowDetailList;
    uint64_t STARTTIME = 9171200;
    uint64_t ENDTIME = 20617000;
    uint64_t LOCK_STARTTIME = 15617000;
    uint64_t LOCK_ENDTIME = 18617000;
    requestParams.startTime = STARTTIME;
    requestParams.endTime = ENDTIME;
    requestParams.timePerPx = 1;
    requestParams.category = "HostToDevice";
    requestParams.rankId = "0";
    requestParams.lockStartTime = LOCK_STARTTIME;
    requestParams.lockEndTime = LOCK_ENDTIME;
    Metadata metadata;
    metadata.metaType = "TEXT";
    metadata.pid = "140836602";
    metadata.tid = "1408366";
    metadata.rankId = "0";
    requestParams.metadataList.emplace_back(metadata);
    uint64_t minTimestamp = 1695115378704489800;
    const int expectSize = 1;
    const uint64_t expectFromTimestamp = 17654000;
    const uint64_t expectToTimestamp = 17902700;
    renderEngine->QueryFlowCategoryEvents(requestParams, minTimestamp, flowDetailList);
    std::string EXPECT_FROM_TID = "1408366";
    EXPECT_EQ(flowDetailList.size(), expectSize); // flowDetailList size = 5
    EXPECT_EQ(flowDetailList[0]->cat, "HostToDevice");
    EXPECT_EQ(flowDetailList[0]->from.timestamp, expectFromTimestamp); // timestamp = 1695115378722143800
    EXPECT_EQ(flowDetailList[0]->from.pid, "140836602");
    EXPECT_EQ(flowDetailList[0]->from.depth, 0);
    EXPECT_EQ(flowDetailList[0]->from.tid, EXPECT_FROM_TID); // from.tid = 1408366
    EXPECT_EQ(flowDetailList[0]->to.timestamp, expectToTimestamp); // to.timestamp = 1695115378722392500
    EXPECT_EQ(flowDetailList[0]->to.pid, "14083661400");
    EXPECT_EQ(flowDetailList[0]->to.depth, 0);
    EXPECT_EQ(flowDetailList[0]->to.tid, "0"); // to.tid = 0
}

TEST_F(TestSuit, QueryFlowCategoryList) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    std::vector<std::string> categories;
    database->QueryFlowCategoryList(categories, "");
    const int thirdNumber = 2;
    EXPECT_EQ(categories.size(), 3); // categories.size = 3
    EXPECT_EQ(categories[0], "HostToDevice");
    EXPECT_EQ(categories[1], "async_npu");
    EXPECT_EQ(categories[thirdNumber], "async_task_queue");
}

TEST_F(TestSuit, QueryThreadsWithEmptyDepth) {
    // request parameters
    Dic::Protocol::UnitThreadsParams request;
    uint64_t STARTTIME = 1695115378734066176;
    uint64_t ENDTIME = 1695115378743155968;
    request.startTime = STARTTIME;
    request.endTime = ENDTIME;
    request.rankId = "0";
    Dic::Protocol::UnitThreadsBody response;
    uint64_t minTimestamp = 0;

    // expected data
    int size = 2;
    std::string title = "AscendCL@aclDestroyTensorDesc";
    uint64_t wallDuration = 42180;
    uint64_t occurrences = 62;
    uint64_t avgWallDuration = 680;
    uint64_t selfTime = 42180;
    Dic::Protocol::Metadata metadata;
    metadata.tid = "1413063";
    metadata.pid = "140836602";
    metadata.metaType.clear();
    request.metadataList.emplace_back(metadata);
    request.rankId = "0";
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    database->QueryThreads(request, response, minTimestamp, {30});

    EXPECT_EQ(response.emptyFlag, false);
    EXPECT_EQ(response.data.size(), size);
    EXPECT_EQ(response.data[0].title, title);
    EXPECT_EQ(response.data[0].wallDuration, wallDuration);
    EXPECT_EQ(response.data[0].occurrences, occurrences);
    EXPECT_EQ(response.data[0].avgWallDuration, avgWallDuration);
    EXPECT_EQ(response.data[0].selfTime, selfTime);
}

TEST_F(TestSuit, QueryThreadsWithDepth0) {
    // request parameters
    Dic::Protocol::UnitThreadsParams request;
    uint64_t STARTTIME = 1695115378734066176;
    uint64_t ENDTIME = 1695115378743155968;
    request.startTime = STARTTIME;
    request.endTime = ENDTIME;
    request.startDepth = "0";
    request.endDepth = "0";
    request.rankId = "0";
    Dic::Protocol::UnitThreadsBody response;
    uint64_t minTimestamp = 0;

    // expected data
    int size = 2;
    std::string title = "AscendCL@aclDestroyTensorDesc";
    uint64_t wallDuration = 42180;
    uint64_t occurrences = 62;
    uint64_t avgWallDuration = 680;
    uint64_t selfTime = 42180;
    Dic::Protocol::Metadata metadata;
    metadata.tid = "1413063";
    metadata.pid = "140836602";
    metadata.metaType.clear();
    request.metadataList.emplace_back(metadata);
    request.rankId = "0";
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    database->QueryThreads(request, response, minTimestamp, {30});

    EXPECT_EQ(response.emptyFlag, false);
    EXPECT_EQ(response.data.size(), size);
    EXPECT_EQ(response.data[0].title, title);
    EXPECT_EQ(response.data[0].wallDuration, wallDuration);
    EXPECT_EQ(response.data[0].occurrences, occurrences);
    EXPECT_EQ(response.data[0].avgWallDuration, avgWallDuration);
    EXPECT_EQ(response.data[0].selfTime, selfTime);
}

TEST_F(TestSuit, QueryThreadDetail) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    // request parameters
    uint64_t STARTTIME = 1695115378728277500;
    uint32_t DEPTH = 7;
    Dic::Protocol::ThreadDetailParams request;
    uint64_t id = 18906;
    request.id = std::to_string(id);
    request.depth = DEPTH;
    request.startTime = STARTTIME;
    int64_t traceId = 41;

    // expected data
    uint64_t selfTime = 0;
    uint64_t duration = 17250;
    std::string args = "{\"correlation_id\":\"2684\"}";
    std::string title = "Enqueue";
    std::string cat = "enqueue";
    Dic::Protocol::UnitThreadDetailBody response;
    request.metaType = "TEXT";
    request.rankId = "0";
    renderEngine->QueryThreadDetail(request, response, traceId);

    EXPECT_EQ(response.emptyFlag, false);
    EXPECT_EQ(response.data.selfTime, selfTime);
    EXPECT_EQ(response.data.duration, duration);
    EXPECT_EQ(response.data.args, args);
    EXPECT_EQ(response.data.title, title);
}

TEST_F(TestSuit, QueryThreadTraces) {
    Dic::Protocol::UnitThreadTracesParams request;
    uint64_t STARTTIME = 1695115378713851200;
    uint64_t ENDTIME = 1695115378728583500;
    request.startTime = STARTTIME;
    request.endTime = ENDTIME;
    request.threadId = "0";
    request.timePerPx = 1;
    Dic::Protocol::UnitThreadTracesBody response;
    uint64_t minTimestamp = 0;
    uint64_t traceId = 30;

    int expectSize = 25;
    std::string expectName = "AscendCL@aclDestroyTensorDesc";
    std::string expectThreadId = request.threadId;
    request.cardId = "0";
    request.metaType = "TEXT";

    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    renderEngine->QueryThreadTraces(request, response, minTimestamp, traceId);

    EXPECT_EQ(response.data[0].size(), expectSize);
}

TEST_F(TestSuit, QueryThreadDetailWithShape) {
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    // request parameters
    uint64_t STARTTIME = 1695115378713661000;
    Dic::Protocol::ThreadDetailParams request;
    uint64_t id = 8;
    request.id = std::to_string(id);
    request.startTime = STARTTIME;
    int64_t traceId = 65;
    // expected data
    uint64_t selfTime = 469541;
    uint64_t duration = 496851;
    std::string args = "{\"Task Type\":\"AI_CORE\",\"Stream Id\":\"16\",\"Task Id\":\"2451\",\"Batch "
                       "Id\":\"0\",\"Subtask Id\":\"4294967295\",\"connection_id\":\"5\"}";
    std::string title = "ZerosLike";
    Dic::Protocol::UnitThreadDetailBody response;
    request.metaType = "TEXT";
    request.rankId = "0";
    renderEngine->QueryThreadDetail(request, response, traceId);
    EXPECT_EQ(response.data.selfTime, selfTime);
    EXPECT_EQ(response.data.duration, duration);
    EXPECT_EQ(response.data.args, args);
    EXPECT_EQ(response.data.title, title);
    EXPECT_EQ(response.data.inputShapes, "\"232138240\"");
    EXPECT_EQ(response.data.inputFormats, "FORMAT_ND");
    EXPECT_EQ(response.data.inputDataTypes, "FLOAT");
    EXPECT_EQ(response.data.outputShapes, "\"232138240\"");
    EXPECT_EQ(response.data.outputDataTypes, "FLOAT");
    EXPECT_EQ(response.data.outputFormats, "FORMAT_ND");
}

TEST_F(TestSuit, QueryExtremumTimestamp) {
    uint64_t expectMin = 1695115378653323500;
    uint64_t expectMax = 1695115379212601200;
    bool expectResult = true;
    uint64_t min;
    uint64_t max;

    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    bool result = database->QueryExtremumTimestamp(min, max);
    EXPECT_EQ(result, expectResult);
    EXPECT_EQ(min, expectMin);
    EXPECT_EQ(max, expectMax);
}

TEST_F(TestSuit, SearchSliceNameCountWithFuzzyMatch) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    uint32_t expectCount = 91;
    SearchCountParams params;
    params.searchContent = "Mul";

    auto count = database->SearchSliceNameCount(params, {});
    EXPECT_EQ(count, expectCount);
}

TEST_F(TestSuit, SearchSliceNameCountWithExactMatch) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    uint32_t expectCount = 4;
    SearchCountParams params;
    params.isMatchExact = true;
    params.searchContent = "Mul";

    auto count = database->SearchSliceNameCount(params, {});
    EXPECT_EQ(count, expectCount);
}

TEST_F(TestSuit, SearchSliceNameCountWithCaseMatch) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    uint32_t expectCount = 55;
    SearchCountParams params;
    params.isMatchCase = true;
    params.searchContent = "Mul";

    auto count = database->SearchSliceNameCount(params, {});
    EXPECT_EQ(count, expectCount);
}

TEST_F(TestSuit, SearchSliceNameCountWithCaseAndExactMatch) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    uint32_t expectCount = 4;
    SearchCountParams params;
    params.isMatchExact = true;
    params.isMatchCase = true;
    params.searchContent = "Mul";

    auto count = database->SearchSliceNameCount(params, {});
    EXPECT_EQ(count, expectCount);
}

TEST_F(TestSuit, SearchSliceName) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SearchSliceParams params;
    params.rankId = "0";
    params.searchContent = "Enqueue";
    int index = 0;
    uint64_t minTimestamp = 0;
    Dic::Protocol::SearchSliceBody body;

    std::string expectPid = "1408366";
    std::string expectTid = "1408366";
    uint64_t expectStartTime = 1695115378713520800;
    uint32_t expectDepth = 3;
    uint64_t expectDuration = 18250;

    database->SearchSliceName(params, index, minTimestamp, body, {});
    EXPECT_EQ(body.pid, expectPid);
    EXPECT_EQ(body.tid, expectTid);
    EXPECT_EQ(body.startTime, expectStartTime);
    EXPECT_EQ(body.depth, expectDepth);
    EXPECT_EQ(body.duration, expectDuration);
}

TEST_F(TestSuit, QueryThreadSameOperatorsDetails) {
    // request parameters
    Protocol::UnitThreadsOperatorsParams requestParam;
    uint64_t START_TIME = 1695115378713505000;
    uint64_t END_TIME = 1695115378714916500;
    uint64_t PAGE_SIZE = 10;
    requestParam.name = "aten::empty";
    requestParam.startTime = START_TIME;
    requestParam.endTime = END_TIME;
    requestParam.orderBy = "timestamp";
    requestParam.current = 1;
    requestParam.pageSize = PAGE_SIZE;
    Protocol::UnitThreadsOperatorsBody responseBody;
    uint64_t minTimestamp = 0;
    std::vector<uint64_t> traceId = {65};

    // response data
    uint64_t TIMESTAMP1 = 1695115378714082800;
    uint64_t DURATION1 = 27310;
    uint64_t TIMESTAMP2 = 1695115378714385200;
    uint64_t DURATION2 = 35710;
    uint64_t COUNT = 2;

    auto database = std::dynamic_pointer_cast<Dic::Module::Timeline::TextTraceDatabase,
        Dic::Module::Timeline::VirtualTraceDatabase>(
        Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0"));
    database->QueryThreadSameOperatorsDetails(requestParam, responseBody, minTimestamp, traceId);
    EXPECT_EQ(responseBody.sameOperatorsDetails[0].timestamp, TIMESTAMP1);
    EXPECT_EQ(responseBody.sameOperatorsDetails[0].duration, DURATION1);
    EXPECT_EQ(responseBody.sameOperatorsDetails[1].timestamp, TIMESTAMP2);
    EXPECT_EQ(responseBody.sameOperatorsDetails[1].duration, DURATION2);
    EXPECT_EQ(responseBody.count, COUNT);
}

TEST_F(TestSuit, SearchAllSlicesDetailsWithFuzzyMatch) {
    uint64_t START_TIME = 1695115378726082200;
    uint64_t DURATION = 3439;
    uint64_t PAGE_SIZE = 10;
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    int expectCount = 91;
    SearchAllSliceParams params;
    params.searchContent = "Mul";
    params.pageSize = PAGE_SIZE;
    params.current = 1;
    params.orderBy = "duration";

    SearchAllSlicesBody body;

    database->SearchAllSlicesDetails(params, body, 0, {});
    EXPECT_EQ(body.searchAllSlices[0].timestamp, START_TIME);
    EXPECT_EQ(body.searchAllSlices[0].duration, DURATION);
    EXPECT_EQ(body.searchAllSlices[0].name, "Mul");
    EXPECT_EQ(body.count, expectCount);
}

TEST_F(TestSuit, QueryEventsViewData4Python) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.processName = "Python (1408366)";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_2;
    params.pid = "1408366";
    params.rankId = "0";

    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 6614;
    const uint64_t EXPECT_START = 1695115378714485500;
    const uint64_t EXPECT_DURATION = 262400;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Api(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);

    auto ptr = dynamic_cast<HostEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "aten::_to_copy");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->tid, "1408366");
    EXPECT_EQ(ptr->pid, "1408366");
    EXPECT_EQ(ptr->processId, "1408366");
    EXPECT_EQ(ptr->threadId, "1408366");
}

TEST_F(TestSuit, QueryEventsViewData4PythonThread) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "1408366";
    params.processName = "Python (1408366)";
    params.tid = "1408366";
    params.threadName = "Thread 1408366";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_2;
    params.rankId = "0";
    params.threadIdList.emplace_back(params.tid);

    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 5644;
    const uint64_t EXPECT_START = 1695115378714532200;
    const uint64_t EXPECT_DURATION = 96310;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Api(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<HostEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "aten::empty_strided");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->tid, "1408366");
    EXPECT_EQ(ptr->pid, "1408366");
    EXPECT_EQ(ptr->processId, "1408366");
    EXPECT_EQ(ptr->threadId, "1408366");
}

TEST_F(TestSuit, QueryEventsViewData4CANN) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "140836602";
    params.processName = "CANN (140836602)";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_2;
    params.rankId = "0";

    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 9523;
    const uint64_t EXPECT_START = 1695115378714600200;
    const uint64_t EXPECT_DURATION = 27010;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Api(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<HostEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "AscendCL@aclrtDestroyEvent");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->tid, "1408366");
    EXPECT_EQ(ptr->pid, "140836602");
    EXPECT_EQ(ptr->processId, "140836602");
    EXPECT_EQ(ptr->threadId, "1408366");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryEventsViewData4CANNThread) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "140836602";
    params.processName = "CANN (140836602)";
    params.tid = "1408366";
    params.threadName = "Thread 1408366";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_2;
    params.rankId = "0";
    params.threadIdList.emplace_back(params.tid);

    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 5524;
    const uint64_t EXPECT_START = 1695115378714991500;
    const uint64_t EXPECT_DURATION = 468180;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Api(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<HostEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "AscendCL@hcom_broadcast_");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->tid, "1408366");
    EXPECT_EQ(ptr->pid, "140836602");
    EXPECT_EQ(ptr->processId, "140836602");
    EXPECT_EQ(ptr->threadId, "1408366");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryEventsViewData4Hardware) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "300";
    params.processName = "Ascend Hardware (300)";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_2;
    params.rankId = "0";

    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 2263;
    const uint64_t EXPECT_START = 1695115378715411000;
    const uint64_t EXPECT_DURATION = 20;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Hardware(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<DeviceEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "Notify Wait");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->threadName, "Stream 22");
    EXPECT_EQ(ptr->rankId, "0");
    EXPECT_EQ(ptr->processId, "300");
    EXPECT_EQ(ptr->threadId, "22");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryEventsViewData4HardwareStream) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "300";
    params.processName = "Ascend Hardware (300)";
    params.tid = "16";
    params.threadName = "Stream 16";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_2;
    params.rankId = "0";
    params.threadIdList.emplace_back(params.tid);
    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 879;
    const uint64_t EXPECT_START = 1695115378723943800;
    const uint64_t EXPECT_DURATION = 369064;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Hardware(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<DeviceEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "Tril");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->threadName, "Stream 16");
    EXPECT_EQ(ptr->rankId, "0");
    EXPECT_EQ(ptr->processId, "300");
    EXPECT_EQ(ptr->threadId, "16");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryEventsViewData4HCCL) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "14083661400";
    params.processName = "HCCL (14083661400)";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_1;
    params.rankId = "0";
    params.orderBy = "duration";
    params.order = "descend";

    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 28;
    const uint64_t EXPECT_START = 1695115378818416800;
    const uint64_t EXPECT_DURATION = 102721612;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Group(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<DeviceEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "hcom_allReduce__459_0");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->threadName, "Group 2 Communication");
    EXPECT_EQ(ptr->rankId, "0");
    EXPECT_EQ(ptr->processId, "14083661400");
    EXPECT_EQ(ptr->threadId, "17");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryEventsViewData4Communication) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "14083661400";
    params.processName = "Communication (14083661400)";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_1;
    params.rankId = "0";
    params.orderBy = "duration";
    params.order = "descend";

    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 28;
    const uint64_t EXPECT_START = 1695115378818416800;
    const uint64_t EXPECT_DURATION = 102721612;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Group(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<DeviceEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "hcom_allReduce__459_0");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->threadName, "Group 2 Communication");
    EXPECT_EQ(ptr->rankId, "0");
    EXPECT_EQ(ptr->processId, "14083661400");
    EXPECT_EQ(ptr->threadId, "17");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryEventsViewData4HCCLGroup) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "14083661400";
    params.processName = "HCCL (14083661400)";
    params.tid = "0";
    params.threadName = "Group 0 Communication";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_1;
    params.rankId = "0";
    params.orderBy = "duration";
    params.order = "descend";
    params.threadIdList.emplace_back(params.tid);
    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 20;
    const uint64_t EXPECT_START = 1695115378715400200;
    const uint64_t EXPECT_DURATION = 4975266;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Group(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<DeviceEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "hcom_broadcast__483_0");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->threadName, "Group 0 Communication");
    EXPECT_EQ(ptr->rankId, "0");
    EXPECT_EQ(ptr->processId, "14083661400");
    EXPECT_EQ(ptr->threadId, "0");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryEventsViewData4CommunicationGroup) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "14083661400";
    params.processName = "Communication (14083661400)";
    params.tid = "0";
    params.threadName = "Group 0 Communication";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_1;
    params.rankId = "0";
    params.orderBy = "duration";
    params.order = "descend";
    params.threadIdList.emplace_back(params.tid);
    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 20;
    const uint64_t EXPECT_START = 1695115378715400200;
    const uint64_t EXPECT_DURATION = 4975266;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Group(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<DeviceEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "hcom_broadcast__483_0");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->threadName, "Group 0 Communication");
    EXPECT_EQ(ptr->rankId, "0");
    EXPECT_EQ(ptr->processId, "14083661400");
    EXPECT_EQ(ptr->threadId, "0");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryEventsViewData4Overlap) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "14083661700";
    params.processName = "Overlap Analysis (14083661700)";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_1;
    params.rankId = "0";
    params.orderBy = "duration";
    params.order = "descend";

    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 1243;
    const uint64_t EXPECT_START = 1695115378818416665;
    const uint64_t EXPECT_DURATION = 102721612;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Overlap(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<DeviceEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "Communication");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->threadName, "Communication");
    EXPECT_EQ(ptr->rankId, "0");
    EXPECT_EQ(ptr->processId, "14083661700");
    EXPECT_EQ(ptr->threadId, "1");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryEventsViewData4OverlapComputing) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EventsViewParams params;
    params.pid = "14083661700";
    params.processName = "Overlap Analysis (14083661700)";
    params.tid = "0";
    params.threadName = "Computing";
    params.pageSize = PAGE_SIZE;
    params.currentPage = CUR_PAGE_1;
    params.rankId = "0";
    params.orderBy = "duration";
    params.order = "descend";
    params.threadIdList.emplace_back(params.tid);
    EventsViewBody body;
    database->QueryEventsViewData(params, body, 0);
    const uint64_t EXPECT_COUNT = 586;
    const uint64_t EXPECT_START = 1695115378795017389;
    const uint64_t EXPECT_DURATION = 7151681;

    EXPECT_EQ(body.count, EXPECT_COUNT);
    CheckEventsViewColumns4Overlap(body);
    EXPECT_EQ(body.eventDetailList.size(), PAGE_SIZE);
    auto ptr = dynamic_cast<DeviceEventDetail *>(body.eventDetailList.at(0).get());
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQ(ptr->name, "Computing");
    EXPECT_EQ(ptr->startTime, EXPECT_START);
    EXPECT_EQ(ptr->duration, EXPECT_DURATION);
    EXPECT_EQ(ptr->threadName, "Computing");
    EXPECT_EQ(ptr->rankId, "0");
    EXPECT_EQ(ptr->processId, "14083661700");
    EXPECT_EQ(ptr->threadId, "0");
    EXPECT_EQ(ptr->depth, 0);
}

TEST_F(TestSuit, QueryP2PCommunicationOpDataTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    std::vector<Protocol::ThreadTraces> p2pOpData{};
    Protocol::ExtremumTimestamp range = {0, (uint64_t)INT64_MAX};
    bool result = database->QueryP2PCommunicationOpData("0", 0, range, p2pOpData);
    EXPECT_EQ(result, true);
    EXPECT_EQ(p2pOpData.size(), 1);
    EXPECT_EQ(p2pOpData.at(0).name, "hcom_send__822_0");
    EXPECT_EQ(p2pOpData.at(0).cname, MARKER_SEND);
}

TEST_F(TestSuit, QuerySliceByTimepointAndNameTest) {
    TextRepository repository;
    SliceQuery sliceQuery;
    sliceQuery.rankId = "0";
    CompeteSliceDomain slice;
    bool res = repository.QuerySliceByTimepointAndName(sliceQuery, slice);
    EXPECT_EQ(res, false);
    sliceQuery.name = "ZerosLike";
    const uint64_t time = 1695115378713662000;
    sliceQuery.timePoint = time;
    res = repository.QuerySliceByTimepointAndName(sliceQuery, slice);
    EXPECT_EQ(res, true);
    const uint64_t expectId = 8;
    EXPECT_EQ(slice.id, expectId);
    const uint64_t expectEnd = 1695115378714157851;
    EXPECT_EQ(slice.endTime, expectEnd);
    const uint64_t expectStart = 1695115378713661000;
    EXPECT_EQ(slice.timestamp, expectStart);
    EXPECT_EQ(slice.pid, "300");
    EXPECT_EQ(slice.tid, "16");
    const uint64_t expectTrack = 7;
    EXPECT_EQ(slice.trackId, expectTrack);
    const uint64_t expectDur = 496851;
    EXPECT_EQ(slice.duration, expectDur);
    EXPECT_EQ(slice.cardId, "0");
}

/**
 * 测试 Text 的 textRepo 转成五种 SliceInterface 的情况
 */
TEST_F(TestSuit, TestDynamicCastOfMultiSliceInterface) {
    TextRepository textRepo;
    // 转 IBaseSliceRepo 成功
    const auto baseSliceRepo = dynamic_cast<IBaseSliceRepo *>(&textRepo);
    EXPECT_NE(baseSliceRepo, nullptr);
    // 转 IPythonFuncSlice 成功
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(&textRepo);
    EXPECT_NE(pythonFuncRepo, nullptr);
    // 转 IFindSliceByNameList 成功
    const auto findSliceByNameList = dynamic_cast<IFindSliceByNameList *>(&textRepo);
    EXPECT_NE(findSliceByNameList, nullptr);
    // 转 IFindSliceByTimepointAndName 成功
    const auto findSliceByTimepointAndName = dynamic_cast<IFindSliceByTimepointAndName *>(&textRepo);
    EXPECT_NE(findSliceByTimepointAndName, nullptr);
    // 转 ITextSlice 成功
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(&textRepo);
    EXPECT_NE(textSliceRepo, nullptr);
}

TEST_F(TestSuit, QueryOverlapAnalysisDataTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    std::vector<Protocol::ThreadTraces> notOverlapData{};
    Dic::Protocol::SystemViewOverallReqParam requestParams;
    requestParams.rankId = "0";
    int intDeviceId = 4;
    uint64_t totalTime = 0;
    ParamsForOAData paramsForOaData = {"", "", 0};
    bool result = database->QueryOverlapAnalysisData(paramsForOaData, intDeviceId, notOverlapData, totalTime);
    EXPECT_EQ(result, false);
    EXPECT_EQ(notOverlapData.empty(), true);
    ParamsForOAData newParamsForOaData = {
        TextSqlConstant::GetOverlapAnalysisTextSqlByType(requestParams), OVERLAP_TYPES.at(2), 0};
    result = database->QueryOverlapAnalysisData(
        newParamsForOaData, intDeviceId, notOverlapData, totalTime); // 2 for not overlap
    EXPECT_EQ(result, true);
    EXPECT_EQ(notOverlapData.size(), 34); // 34
}

TEST_F(TestSuit, QueryCommunicationGroupMapTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    std::map<std::string, std::string> groupMap{};
    int intDeviceId = 24;
    bool result = database->QueryCommunicationGroupMap("", intDeviceId, groupMap);
    EXPECT_EQ(result, false);
    EXPECT_EQ(groupMap.empty(), true);
    result = database->QueryCommunicationGroupMap(QUERY_COMMUNICATION_GROUP_MAP_TEXT_SQL, intDeviceId, groupMap);
    EXPECT_EQ(result, true);
    EXPECT_EQ(groupMap.size(), 21); // 21
    EXPECT_EQ(groupMap.at("14083661400@17"), "Group 2 Communication");
}

TEST_F(TestSuit, QueryByteAlignmentAnalyzerRawDataTest) {
    auto database = dynamic_pointer_cast<TextTraceDatabase>(
        Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0"));
    std::vector<std::pair<std::string, std::string>> rawData;
    bool result = database->QueryByteAlignmentAnalyzerRawData(rawData);
    ASSERT_TRUE(result);
    ASSERT_EQ(rawData.size(), 282); // 282
    EXPECT_EQ(rawData[0].first, "Memcpy");
    EXPECT_EQ(rawData[1].first, "hcom_broadcast__483_0");
    EXPECT_EQ(rawData[2].first, "Memcpy"); // 2
    EXPECT_EQ(rawData[3].first, "hcom_broadcast__483_1"); // 3
    EXPECT_EQ(rawData[4].first, "Memcpy"); // 4
    EXPECT_EQ(rawData[13].first, "Reduce_Inline"); // 13
    EXPECT_EQ(rawData[281].first, "Memcpy"); // 281
}
