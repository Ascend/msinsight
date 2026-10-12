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
#ifndef TIMELINE_HANDLER_SUCCESS_TEST_SUPPORT_H
#define TIMELINE_HANDLER_SUCCESS_TEST_SUPPORT_H

#include <gtest/gtest.h>
#include <any>
#include <atomic>
#include <future>
#include <shared_mutex>
#include <sstream>

// Limit access to fixture setup/restore; requests still enter the public handlers.
#define private public
#include "DataBaseManager.h"
#include "SystemViewOverallRepoFactory.h"
#include "SystemViewOverallCacheManager.h"
#include "TraceTime.h"
#include "WsSessionManager.h"
#undef private
#include "QueryMemcpyDetailHandler.h"
#include "QueryOverallMoreDetailsHandler.h"
#include "QuerySystemViewOverallHandler.h"

namespace Dic::Module::Timeline {
namespace {
class OverallRepositoryFixture final : public SystemViewOverallRepoInterface {
  public:
    std::vector<OverallTmpInfo> overlaps;
    std::vector<OverallTmpInfo> kernels;
    std::vector<SameOperatorsDetails> communication;
    SystemViewOverallReqParam lastParams;
    unsigned overlapQueries = 0;
    unsigned computingQueries = 0;
    unsigned communicationQueries = 0;
    uint64_t communicationOffset = 0;
    bool computingSuccess = true;
    bool communicationSuccess = true;

    std::vector<OverallTmpInfo> QueryOverlapAnalysisDataForOverallMetric(
        const SystemViewOverallReqParam &params, const std::shared_ptr<VirtualTraceDatabase> &) override {
        ++overlapQueries;
        lastParams = params;
        return overlaps;
    }
    bool QueryDataForComputingOverallMetric(const SystemViewOverallReqParam &params, SystemViewOverallHelper &helper,
        const std::shared_ptr<VirtualTraceDatabase> &) override {
        ++computingQueries;
        lastParams = params;
        helper.kernelEvents = kernels;
        return computingSuccess;
    }
    void QueryCommunicationOverlapOverallInfos(const SystemViewOverallReqParam &, SystemViewOverallHelper &,
        std::vector<SystemViewOverallRes> &, const std::shared_ptr<VirtualTraceDatabase> &) override {}
    bool QueryCommunicationOpsTimeDataByGroupName(const SystemViewOverallReqParam &params, uint64_t offset,
        const std::vector<ThreadTraces> &, std::vector<SameOperatorsDetails> &details,
        const std::shared_ptr<VirtualTraceDatabase> &) override {
        ++communicationQueries;
        lastParams = params;
        communicationOffset = offset;
        details = communication;
        return communicationSuccess;
    }
};

class TimelineHandlerSuccessTest : public ::testing::Test {
  protected:
    class CaptureSession final : public Server::WsSession {
      public:
        std::vector<std::unique_ptr<Response>> responses;
        void Start() override { status = Status::STARTED; }
        Status GetStatus() const override { return status; }
        void OnResponse(std::unique_ptr<Response> response) override { responses.push_back(std::move(response)); }
        void SendBaseResponse(std::unique_ptr<Response> response) override { OnResponse(std::move(response)); }
        void OnEvent(std::unique_ptr<Event>) override {}
        void SetStatus(Status value) override { status = value; }
        void WaitForExit(int) override {}

      private:
        Status status = Status::STARTED;
    };

    void SetUp() override {
        // Access request errors through the module API, including in DLL builds.
        Response previousError;
        ModuleRequestHandler::SetResponseErrorFromRequestContext(previousError);
        savedError = previousError.error.value_or(ErrorMessage{});
        ModuleRequestHandler::ResetRequestContextError();
        auto &sessions = Server::WsSessionManager::Instance();
        {
            std::lock_guard<std::mutex> lock(sessions.sessionMutex);
            savedSession = std::exchange(sessions.session, capture);
        }
        auto &time = TraceTime::Instance();
        {
            std::lock_guard<std::mutex> lock(time.mutex);
            savedMin = time.minTimestamp;
            savedMax = time.maxTimestamp;
            time.minTimestamp = BASE_TIME;
            time.maxTimestamp = BASE_TIME + 100000;
        }
        auto factory = SystemViewOverallRepoFactory::Instance();
        const auto existing = factory->systemViewOverallRepoMap.find(DataType::TEXT);
        hadRepository = existing != factory->systemViewOverallRepoMap.end();
        if (hadRepository) {
            savedRepository = existing->second;
        }
        factory->systemViewOverallRepoMap[DataType::TEXT] = repository;

        static std::atomic<unsigned> sequence{0};
        fileId = "timeline-handler-success-ut-" + std::to_string(++sequence);
        rankId = fileId + "-rank";
        uri = "file:msinsight_overlap_" + fileId + "?mode=memory&cache=shared";
        auto &manager = DataBaseManager::Instance();
        manager.SetDataType(DataType::TEXT, fileId);
        manager.SetRankIdFileIdMapping(rankId, fileId);
        manager.UpdateRankIdToDeviceId(fileId, rankId, "3");
        ASSERT_TRUE(manager.CreateDerivedTraceConnectionPool(fileId, uri, true));
        auto database = manager.GetTraceDatabaseByFileId(fileId);
        ASSERT_NE(database, nullptr);
        ASSERT_TRUE(database->UpdateValueIntoStatusInfoTable(OVERLAP_ANALYSIS_UNIT, FINISH_STATUS));
        for (double duration : {30.0, 60.0, 10.0}) {
            OverallTmpInfo info;
            info.duration = duration;
            repository->overlaps.push_back(info);
        }
    }

    void TearDown() override {
        auto &manager = DataBaseManager::Instance();
        manager.ReleaseDatabaseByFileId(fileId);
        {
            std::lock_guard<std::recursive_mutex> lock(manager.mutex);
            manager.dataTypeMap.erase(fileId);
            manager.dbMutexMap.erase(fileId);
            manager.databasePathSet.erase(uri);
        }
        auto &cache = SystemViewOverallCacheManager::Instance();
        {
            std::unique_lock<std::shared_mutex> lock(cache.sharedMutex);
            cache.systemViewOverallCacheMap.erase(fileId);
        }
        auto factory = SystemViewOverallRepoFactory::Instance();
        if (hadRepository) {
            factory->systemViewOverallRepoMap[DataType::TEXT] = savedRepository;
        } else {
            factory->systemViewOverallRepoMap.erase(DataType::TEXT);
        }
        auto &time = TraceTime::Instance();
        {
            std::lock_guard<std::mutex> lock(time.mutex);
            time.minTimestamp = savedMin;
            time.maxTimestamp = savedMax;
        }
        auto &sessions = Server::WsSessionManager::Instance();
        {
            std::lock_guard<std::mutex> lock(sessions.sessionMutex);
            sessions.session = std::move(savedSession);
        }
        ModuleRequestHandler::SetRequestContextError(savedError);
    }

    template <typename RequestType> std::unique_ptr<RequestType> RequestForTest() const {
        auto request = std::make_unique<RequestType>();
        request->id = 37;
        request->fileId = fileId;
        request->params.rankId = rankId;
        request->params.page.current = 1;
        request->params.page.pageSize = 2;
        request->params.order.orderBy = "startTime";
        request->params.order.orderType = "ascend";
        return request;
    }

    template <typename ResponseType> ResponseType *LastResponse(bool success = true) {
        EXPECT_EQ(capture->responses.size(), 1U);
        if (capture->responses.size() != 1) {
            return nullptr;
        }
        auto *response = dynamic_cast<ResponseType *>(capture->responses.front().get());
        EXPECT_NE(response, nullptr);
        if (response != nullptr) {
            EXPECT_EQ(response->requestId, 37U);
            EXPECT_EQ(response->result, success);
        }
        return response;
    }

    void SeedComputing() {
        for (const auto &entry : std::vector<std::pair<uint64_t, double>>{{300, 2.0}, {100, 4.0}, {200, 1.0}}) {
            OverallTmpInfo event;
            event.opName = "Matmul_" + std::to_string(entry.first);
            event.opType = "MatMul";
            event.startTime = BASE_TIME + entry.first;
            event.duration = entry.second;
            repository->kernels.push_back(event);
        }
    }

    void SeedCommunication() {
        for (uint64_t timestamp : {30, 10, 20}) {
            SameOperatorsDetails event;
            event.name = "AllReduce_" + std::to_string(timestamp);
            event.timestamp = timestamp;
            event.duration = timestamp * 2;
            repository->communication.push_back(event);
        }
    }

    void SeedMemcpy() {
        auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(fileId);
        ASSERT_NE(database, nullptr);
        ASSERT_TRUE(database->ExecSql("CREATE TABLE thread(track_id INTEGER, tid TEXT);"));
        ASSERT_TRUE(database->ExecSql("CREATE TABLE slice(id INTEGER, name TEXT, args TEXT, timestamp INTEGER, "
                                      "duration INTEGER, end_time INTEGER, track_id INTEGER);"));
        ASSERT_TRUE(database->ExecSql("INSERT INTO thread VALUES(1,'7'),(2,'8');"));
        ASSERT_TRUE(database->ExecSql("INSERT INTO slice VALUES"
                                      "(11,'MEMCPY_ASYNC','{\"operation\":\"H2D\",\"size(B)\":64}',1100,20,1120,1),"
                                      "(12,'MEMCPY_ASYNC','{\"operation\":\"D2H\",\"size(B)\":256}',1200,40,1240,1),"
                                      "(13,'MEMCPY_ASYNC','{\"operation\":\"H2D\",\"size(B)\":128}',1300,30,1330,2),"
                                      "(14,'Kernel','{}',1150,10,1160,1);"));
    }

    void SeedCachedOverall() {
        SystemViewOverallRes cached;
        cached.name = "cached overall";
        cached.totalTime = 777;
        SystemViewOverallCacheManager::Instance().SetOverallData(fileId, {cached});
    }

    static constexpr uint64_t BASE_TIME = 1000;
    std::string fileId;
    std::string rankId;
    std::string uri;
    std::shared_ptr<CaptureSession> capture = std::make_shared<CaptureSession>();
    std::shared_ptr<OverallRepositoryFixture> repository = std::make_shared<OverallRepositoryFixture>();

  private:
    std::shared_ptr<Server::WsSession> savedSession;
    std::shared_ptr<SystemViewOverallRepoInterface> savedRepository;
    bool hadRepository = false;
    ErrorMessage savedError;
    uint64_t savedMin = 0;
    uint64_t savedMax = 0;
};
} // namespace
} // namespace Dic::Module::Timeline

#endif // TIMELINE_HANDLER_SUCCESS_TEST_SUPPORT_H
