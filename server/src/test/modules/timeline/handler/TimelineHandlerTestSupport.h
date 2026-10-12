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

#ifndef MSINSIGHT_TIMELINE_HANDLER_TEST_SUPPORT_H
#define MSINSIGHT_TIMELINE_HANDLER_TEST_SUPPORT_H

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <sqlite3.h>
#include "DataBaseManager.h"
#include "SearchSliceCacheManager.h"
#include "SliceCacheManager.h"
#include "TimelineErrorManager.h"
#include "TrackInfoManager.h"
#include "TraceTime.h"
#include "WsSession.h"
#define private public
#include "WsSessionManager.h"
#undef private

namespace Dic::Module::Timeline::Test {
class CapturingTimelineSession : public Server::WsSession {
  public:
    void Start() override { status = Status::STARTED; }
    Status GetStatus() const override { return status; }
    void OnResponse(std::unique_ptr<Protocol::Response> value) override { response = std::move(value); }
    void SendBaseResponse(std::unique_ptr<Protocol::Response> value) override { response = std::move(value); }
    void OnEvent(std::unique_ptr<Protocol::Event>) override {}
    void SetStatus(Status value) override { status = value; }
    void WaitForExit(int = 10000) override {}
    template <typename T> const T *Get() const { return dynamic_cast<const T *>(response.get()); }
    std::unique_ptr<Protocol::Response> response;

  private:
    Status status = Status::STARTED;
};

class TimelineHandlerDatabaseTest : public testing::Test {
  protected:
    void SetUp() override {
        session = std::make_shared<CapturingTimelineSession>();
        session->Start();
        // AddSession starts its argument; restoring through it would restart the previous session's threads.
        {
            auto &manager = Server::WsSessionManager::Instance();
            std::lock_guard<std::mutex> lock(manager.sessionMutex);
            previousSession = std::move(manager.session);
            manager.session = session;
        }
        ModuleRequestHandler::ResetRequestContextError();
        DataBaseManager::Instance().Clear();
        SearchSliceCacheManager::Instance().clearAll();
        SliceCacheManager::Instance().Clear();
        TrackInfoManager::Instance().Reset();
        TraceTime::Instance().Reset();
        TraceTime::Instance().UpdateTime(100, 1000);
        std::error_code error;
        const auto temporaryDirectory = std::filesystem::temp_directory_path(error);
        ASSERT_FALSE(error) << error.message();
        directory = temporaryDirectory /
            ("timeline-handler-ut-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        directoryCreated = std::filesystem::create_directory(directory, error);
        ASSERT_FALSE(error) << error.message();
        ASSERT_TRUE(directoryCreated);
    }
    void TearDown() override {
        session->response.reset();
        DataBaseManager::Instance().Clear();
        SearchSliceCacheManager::Instance().clearAll();
        SliceCacheManager::Instance().Clear();
        TrackInfoManager::Instance().Reset();
        TraceTime::Instance().Reset();
        ModuleRequestHandler::ResetRequestContextError();
        {
            auto &manager = Server::WsSessionManager::Instance();
            std::lock_guard<std::mutex> lock(manager.sessionMutex);
            manager.session = std::move(previousSession);
        }
        if (directoryCreated) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            EXPECT_FALSE(error) << error.message();
        }
    }
    std::string AddDatabase(const std::string &rank, DataType type = DataType::TEXT) {
        const auto path = (directory / (rank + ".db")).string();
        sqlite3 *raw = nullptr;
        const auto opened = sqlite3_open(path.c_str(), &raw);
        EXPECT_EQ(opened, SQLITE_OK);
        if (raw) {
            sqlite3_close(raw);
        }
        if (opened != SQLITE_OK) {
            return {};
        }
        auto &manager = DataBaseManager::Instance();
        manager.SetDataType(type, path);
        EXPECT_TRUE(manager.CreateTraceConnectionPool(rank, path));
        auto database = manager.GetTraceDatabaseByRankId(rank);
        EXPECT_NE(database, nullptr);
        if (!database) {
            return {};
        }
        if (type == DataType::TEXT) {
            auto text = std::dynamic_pointer_cast<TextTraceDatabase>(database);
            EXPECT_NE(text, nullptr);
            if (!text) {
                return {};
            }
            EXPECT_TRUE(text->CreateTable());
        }
        return path;
    }
    void Exec(const std::string &rank, const std::string &sql) {
        auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(rank);
        ASSERT_NE(database, nullptr);
        ASSERT_TRUE(database->ExecSql(sql)) << sql;
    }
    void AddSlices(const std::string &rank) {
        Exec(rank,
            "INSERT INTO process(pid,process_name,label,process_sort_index) VALUES('10','Python','CPU',0);"
            "INSERT INTO thread(track_id,tid,pid,thread_name,thread_sort_index) VALUES(1,'11','10','worker',0);"
            "INSERT INTO slice(id,timestamp,duration,end_time,name,depth,track_id,cat,args,cname,flag_id,group_id) "
            "VALUES"
            "(1,120,20,140,'MatMul',0,1,'','','','',''),"
            "(2,180,30,210,'MatMulGrad',1,1,'','','','',''),"
            "(3,250,10,260,'Add',0,1,'','','','','');");
        TrackInfoManager::Instance().UpdateTrackIdMap(rank, {{1, {"11", "10"}}});
        EXPECT_EQ(TrackInfoManager::Instance().GetTrackId(rank, "10", "11"), 1U);
    }
    void ResetResponse() {
        session->response.reset();
        ModuleRequestHandler::ResetRequestContextError();
    }
    template <typename T> void ExpectFailure(ErrorCode code) {
        const auto *response = session->Get<T>();
        ASSERT_NE(response, nullptr);
        EXPECT_FALSE(response->result);
        ASSERT_TRUE(response->error.has_value());
        EXPECT_EQ(response->error->code, static_cast<int>(code));
        EXPECT_EQ(response->error->message, GetErrorMessage(code));
    }
    std::filesystem::path directory;
    bool directoryCreated = false;
    std::shared_ptr<CapturingTimelineSession> session;
    std::shared_ptr<Server::WsSession> previousSession;
};
} // namespace Dic::Module::Timeline::Test
#endif
