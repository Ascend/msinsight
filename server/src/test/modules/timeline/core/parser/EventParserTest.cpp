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
#include <gmock/gmock.h>
#include "TextTraceDatabase.h"
#include "../../../defaultMock/MockFileReader.h"
#include "EventParser.h"
#include "../../../../DatabaseTestCaseMockUtil.h"
#include "SliceTable.h"
#include "ParserStatusManager.h"
#include "SimulationSliceCacheManager.h"
#include "ThreadTable.h"
#include "ProcessTable.h"
#include "FlowTable.h"
#include "EventUtil.h"
#include "JsonUtil.h"
#include "TrackInfoManager.h"
using ::testing::ByMove;
using ::testing::Return;
using namespace Dic::Module::Timeline;
class EventParserTest : public ::testing::Test {
protected:
    class MockDatabase : public TextTraceDatabase {
    public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : TextTraceDatabase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr)
        {
            isOpen = true;
            db = dbPtr;
            path = ":memory:";
        }
        ~MockDatabase() override
        {
            isOpen = false;
        }
    };
    const int64_t startPosition = 12;
    const int64_t endPosition = 98;
    const std::string filePath = "hhh";
    const std::string fileId = "lll";
    std::string sliceTableSql =
        "CREATE TABLE slice (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, duration INTEGER, name TEXT, "
        "depth INTEGER, track_id INTEGER, cat TEXT, args TEXT, cname TEXT, end_time INTEGER, flag_id TEXT, "
        "group_id TEXT);";
    std::unique_ptr<MockFileReader> mockFileReader = std::make_unique<MockFileReader>();
    std::recursive_mutex sqlMutex;
    std::shared_ptr<MockDatabase> mockDatabase = std::make_unique<MockDatabase>(sqlMutex);
    void SetUp() override
    {
        ParserStatusManager::Instance().ClearAllParserStatus();
        SimulationSliceCacheManager::Instance().ClearAll();
        TrackInfoManager::Instance().Reset();
    }

    void TearDown() override
    {
        ParserStatusManager::Instance().ClearAllParserStatus();
        SimulationSliceCacheManager::Instance().ClearAll();
        TrackInfoManager::Instance().Reset();
    }
};

TEST_F(EventParserTest, TestEventUtilParseGroupId) {
    document_t json;
    json.Parse(R"({"ph":"X","ts":1,"dur":2,"name":"slice","pid":1,"tid":2,"group_id":"groupA"})");
    auto event = EventUtil::Instance().FromJson(json, "X");
    ASSERT_NE(event, nullptr);
    auto &slice = dynamic_cast<Trace::Slice &>(*event);
    EXPECT_EQ(slice.groupId, "groupA");
}

TEST_F(EventParserTest, TestEventUtilParseSimulationGroupId) {
    document_t json;
    json.Parse(R"({"ph":"SX","ts":1,"dur":2,"name":"slice","pid":"p","tid":"t","group_id":"groupA"})");
    auto event = EventUtil::Instance().FromJson(json, "SX");
    ASSERT_NE(event, nullptr);
    auto &slice = dynamic_cast<Trace::Slice &>(*event);
    EXPECT_EQ(slice.groupId, "groupA");
}

TEST_F(EventParserTest, TestEventUtilParseSimulationBeginGroupId) {
    document_t json;
    json.Parse(R"({"ph":"SB","ts":1,"name":"SET_FLAG","pid":"p","tid":"t","id":1,"group_id":"groupA"})");
    auto event = EventUtil::Instance().FromJson(json, "SB");
    ASSERT_NE(event, nullptr);
    auto &slice = dynamic_cast<Trace::Slice &>(*event);
    EXPECT_EQ(slice.groupId, "groupA");
}

class EventParserMock : public EventParser {
public:
    EventParserMock(const std::string &filePath, const std::string &fileId,
        std::shared_ptr<TextTraceDatabase> textDatabase)
        : EventParser(filePath, fileId, textDatabase){};
    void SetFileReaderAndDatabase(std::unique_ptr<MockFileReader> fileReaderPtr)
    {
        fileReader = std::move(fileReaderPtr);
    }
};

/**
 * 测试解析type为X的json
 */
TEST_F(EventParserTest, TestSliceParse)
{
    std::string jsonContent = "[{\"name\": \"PROFILING_DISABLE\", \"pid\": 2094647552, \"tid\": 0, \"ts\": "
        "\"1718180920003154.317\", \"dur\": "
        "0, \"args\": {\"Model Id\": 4294967295, \"Task Type\": \"PLACE_HOLDER_SQE\", \"Physic Stream Id\": 0, \"Task "
        "Id\": 17, \"Batch Id\": 0, \"Subtask Id\": 4294967295, \"connection_id\": -1}, \"ph\": \"X\"}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    const uint64_t exceptTime = 1718180920003154317;
    const uint64_t exceptDuration = 0;
    const uint64_t exceptTrackId = 1;
    const std::string name = "PROFILING_DISABLE";
    EXPECT_EQ(slicePOs.size(), expectSize);
    EXPECT_EQ(slicePOs[first].timestamp, exceptTime);
    EXPECT_EQ(slicePOs[first].duration, exceptDuration);
    EXPECT_EQ(slicePOs[first].name, name);
    EXPECT_EQ(slicePOs[first].trackId, exceptTrackId);
    EXPECT_EQ(slicePOs[first].endTime, exceptTime + exceptDuration);
    EXPECT_EQ(slicePOs[first].args.empty(), false);
}

/**
 * 测试解析type为X的json,无ph为M的数据,正常生成线程和进程信息
 */
TEST_F(EventParserTest, TestSliceParseWithoutPhIsMThenCheckThreadAndProcess)
{
    std::string jsonContent = "[{\"name\": \"PROFILING_DISABLE\", \"pid\": 2094647552, \"tid\": 8, \"ts\": "
        "\"1718180920003154.317\", \"dur\": "
        "0, \"args\": {\"Model Id\": 4294967295, \"Task Type\": \"PLACE_HOLDER_SQE\", \"Physic Stream Id\": 0, \"Task "
        "Id\": 17, \"Batch Id\": 0, \"Subtask Id\": 4294967295, \"connection_id\": -1}, \"ph\": \"X\"}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .ExcuteQuery(dbPtr, threadPOS);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    EXPECT_EQ(threadPOS.size(), expectSize);
    EXPECT_EQ(1, threadPOS[first].trackId);
    EXPECT_EQ("8", threadPOS[first].tid);
    EXPECT_EQ("8", threadPOS[first].threadName);
    EXPECT_EQ("2094647552", threadPOS[first].pid);
    Dic::Module::Timeline::ProcessTable processTable;
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME).ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ("2094647552", processPOS[first].pid);
    EXPECT_EQ("2094647552", processPOS[first].processName);
}

/**
 * 测试解析type为X的json,有ph为M的数据,但没有tid信息
 */
TEST_F(EventParserTest, TestSliceParseWithoutTidThenCheckThreadAndProcess)
{
    std::string jsonContent =
        "[{\"name\": \"process_name\", \"pid\": 2094647552, \"tid\": 0, \"args\": {\"name\": \"LLLL\"}, \"ph\": "
        "\"M\"},{\"name\": \"PROFILING_DISABLE\", \"pid\": 2094647552, \"tid\": 8, \"ts\": "
        "\"1718180920003154.317\", \"dur\": "
        "0, \"args\": {\"Model Id\": 4294967295, \"Task Type\": \"PLACE_HOLDER_SQE\", \"Physic Stream Id\": 0, \"Task "
        "Id\": 17, \"Batch Id\": 0, \"Subtask Id\": 4294967295, \"connection_id\": -1}, \"ph\": \"X\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .ExcuteQuery(dbPtr, threadPOS);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    EXPECT_EQ(threadPOS.size(), expectSize);
    EXPECT_EQ(1, threadPOS[first].trackId);
    EXPECT_EQ("8", threadPOS[first].tid);
    EXPECT_EQ("8", threadPOS[first].threadName);
    EXPECT_EQ("2094647552", threadPOS[first].pid);
    Dic::Module::Timeline::ProcessTable processTable;
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME).ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ("2094647552", processPOS[first].pid);
    EXPECT_EQ("LLLL", processPOS[first].processName);
}

/**
 * 测试解析type为X的json,有ph为M的数据,但没有pid信息
 */
TEST_F(EventParserTest, TestSliceParseWithoutPidThenCheckThreadAndProcess)
{
    std::string jsonContent = "[{\"name\": \"PROFILING_DISABLE\", \"pid\": 2094647552, \"tid\": 8, \"ts\": "
        "\"1718180920003154.317\", \"dur\": "
        "0, \"args\": {\"Model Id\": 4294967295, \"Task Type\": \"PLACE_HOLDER_SQE\", \"Physic Stream Id\": 0, \"Task "
        "Id\": 17, \"Batch Id\": 0, \"Subtask Id\": 4294967295, \"connection_id\": -1}, \"ph\": \"X\"},{\"name\": "
        "\"thread_name\", \"pid\": 2094647552, \"tid\": 8, \"args\": {\"name\": \"Stream 20\"}, \"ph\": \"M\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .ExcuteQuery(dbPtr, threadPOS);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    EXPECT_EQ(threadPOS.size(), expectSize);
    EXPECT_EQ(1, threadPOS[first].trackId);
    EXPECT_EQ("8", threadPOS[first].tid);
    EXPECT_EQ("Stream 20", threadPOS[first].threadName);
    EXPECT_EQ("2094647552", threadPOS[first].pid);
    Dic::Module::Timeline::ProcessTable processTable;
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME).ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ("2094647552", processPOS[first].pid);
    EXPECT_EQ("2094647552", processPOS[first].processName);
}

/**
 * 测试解析type为X的json,pid和tid信息都有
 */
TEST_F(EventParserTest, TestSliceParseWithPidAndTidThenCheckThreadAndProcess)
{
    std::string jsonContent =
        "[{\"name\": \"process_name\", \"pid\": 2094647552, \"tid\": 8, \"args\": {\"name\": \"HCCL\"}, \"ph\": "
        "\"M\"},{\"name\": \"PROFILING_DISABLE\", \"pid\": 2094647552, \"tid\": 8, \"ts\": "
        "\"1718180920003154.317\", \"dur\": "
        "0, \"args\": {\"Model Id\": 4294967295, \"Task Type\": \"PLACE_HOLDER_SQE\", \"Physic Stream Id\": 0, \"Task "
        "Id\": 17, \"Batch Id\": 0, \"Subtask Id\": 4294967295, \"connection_id\": -1}, \"ph\": \"X\"},{\"name\": "
        "\"thread_name\", \"pid\": 2094647552, \"tid\": 8, \"args\": {\"name\": \"Stream 20\"}, \"ph\": \"M\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .ExcuteQuery(dbPtr, threadPOS);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    EXPECT_EQ(threadPOS.size(), expectSize);
    EXPECT_EQ(1, threadPOS[first].trackId);
    EXPECT_EQ("8", threadPOS[first].tid);
    EXPECT_EQ("Stream 20", threadPOS[first].threadName);
    EXPECT_EQ("2094647552", threadPOS[first].pid);
    Dic::Module::Timeline::ProcessTable processTable;
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME).ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ("2094647552", processPOS[first].pid);
    EXPECT_EQ("HCCL", processPOS[first].processName);
}

/**
 * json文件格式不正确
 */
TEST_F(EventParserTest, TestParseWhenJsonFormatIsWrongThenReturnFalse)
{
    std::string jsonContent = "hhhhhhjjj";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    bool result = eventParserMock.Parse(startPosition, endPosition);
    EXPECT_EQ(false, result);
}

/**
 * 读取文件失败
 */
TEST_F(EventParserTest, TestParseWhenReadFailedThenReturnFalse)
{
    std::string jsonContent;
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    bool result = eventParserMock.Parse(startPosition, endPosition);
    std::string error = eventParserMock.GetError();
    EXPECT_EQ("Failed to read file when parse. file path is: hhh", error);
    EXPECT_EQ(false, result);
}

/**
 * json不是数组格式
 */
TEST_F(EventParserTest, TestParseWhenJsonIsNotArrayThenReturnFalse)
{
    const std::string jsonContent = "{\"name\": \"PROFILING_DISABLE\", \"pid\": 2094647552, \"tid\": 0, \"ts\": "
        "\"1718180920003154.317\", \"dur\": "
        "0, \"args\": {\"Model Id\": 4294967295, \"Task Type\": \"PLACE_HOLDER_SQE\", \"Physic Stream Id\": 0, \"Task "
        "Id\": 17, \"Batch Id\": 0, \"Subtask Id\": 4294967295, \"connection_id\": -1}, \"ph\": \"X\"}";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    bool result = eventParserMock.Parse(startPosition, endPosition);
    std::string error = eventParserMock.GetError();
    EXPECT_EQ("json is not an array.", error);
    EXPECT_EQ(false, result);
}

/**
 * 解析任务不是running状态
 */
TEST_F(EventParserTest, TestParseWhenTaskIsNotRunningThenReturnFalse)
{
    const std::string jsonContent = "[{\"name\": \"PROFILING_DISABLE\", \"pid\": 2094647552, \"tid\": 0, \"ts\": "
        "\"1718180920003154.317\", \"dur\": "
        "0, \"args\": {\"Model Id\": 4294967295, \"Task Type\": \"PLACE_HOLDER_SQE\", \"Physic Stream Id\": 0, \"Task "
        "Id\": 17, \"Batch Id\": 0, \"Subtask Id\": 4294967295, \"connection_id\": -1}, \"ph\": \"X\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::UN_KNOW);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    bool result = eventParserMock.Parse(startPosition, endPosition);
    std::string error = eventParserMock.GetError();
    EXPECT_EQ("", error);
    EXPECT_EQ(false, result);
}

/**
 * json中没有ph字段
 */
TEST_F(EventParserTest, TestParseWhenJsonNotHavePhThenNotThrowException)
{
    const std::string jsonContent = "[{\"name\": \"PROFILING_DISABLE\", \"pid\": 2094647552, \"tid\": 0, \"ts\": "
        "\"1718180920003154.317\", \"dur\": "
        "0, \"args\": {\"Model Id\": 4294967295, \"Task Type\": \"PLACE_HOLDER_SQE\", \"Physic Stream Id\": 0, \"Task "
        "Id\": 17, \"Batch Id\": 0, \"Subtask Id\": 4294967295, \"connection_id\": -1}}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    EXPECT_NO_THROW(eventParserMock.Parse(startPosition, endPosition));
}

/**
 * json中ph字段没有注册
 */
TEST_F(EventParserTest, TestParseWhenJsonNotRegisiterPhThenNotThrowException)
{
    const std::string jsonContent = "[{\"name\": \"PROFILING_DISABLE\", \"pid\": 2094647552, \"tid\": 0, \"ts\": "
        "\"1718180920003154.317\", \"dur\": "
        "0, \"args\": {\"Model Id\": 4294967295, \"Task Type\": \"PLACE_HOLDER_SQE\", \"Physic Stream Id\": 0, \"Task "
        "Id\": 17, \"Batch Id\": 0, \"Subtask Id\": 4294967295, \"connection_id\": -1}, \"ph\": \"Xllllllll\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    EXPECT_NO_THROW(eventParserMock.Parse(startPosition, endPosition));
}

/**
 * 测试解析type为M的json,修改processName
 */
TEST_F(EventParserTest, TestMParseModifyProcessName)
{
    std::string jsonContent =
        "[{\"name\": \"process_name\", \"pid\": 2094647552, \"tid\": 8, \"args\": {\"name\": \"HCCL\"}, \"ph\": "
        "\"M\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    Dic::Module::Timeline::ProcessTable processTable;
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME).ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ("2094647552", processPOS[first].pid);
    EXPECT_EQ("HCCL", processPOS[first].processName);
}

/**
 * 测试解析type为M的json,修改processLabel
 */
TEST_F(EventParserTest, TestMParseModifyProcessLabel)
{
    std::string jsonContent =
        "[{\"name\": \"process_labels\", \"pid\": 2094647552, \"tid\": 8, \"args\": {\"labels\": \"CPU\"}, \"ph\": "
        "\"M\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    Dic::Module::Timeline::ProcessTable processTable;
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::LABEL).ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ("2094647552", processPOS[first].pid);
    EXPECT_EQ("CPU", processPOS[first].label);
}

/**
 * 测试解析type为M的json, json 的 name 非法
 */
TEST_F(EventParserTest, TestMParseModifyProcessLabelWithInvalidName)
{
    std::string jsonContent =
        "[{\"name\": \"invalid_name\\ta\", \"pid\": 2094647552, \"tid\": 8, \"args\": {\"labels\": \"CPU\"}, \"ph\": "
        "\"M\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
}

/**
 * 测试解析type为M的json,修改process_sort_index
 */
TEST_F(EventParserTest, TestMParseModifyProcessSortIndex)
{
    std::string jsonContent = "[{\"name\": \"process_sort_index\", \"pid\": 2094647552, \"tid\": 8, \"args\": "
        "{\"sort_index\": \"555\"}, \"ph\": "
        "\"M\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    const uint64_t expectSize = 1;
    const uint64_t expectIndex = 555;
    const uint64_t first = 0;
    Dic::Module::Timeline::ProcessTable processTable;
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_SORT_INDEX).ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ("2094647552", processPOS[first].pid);
    EXPECT_EQ(expectIndex, processPOS[first].processSortIndex);
}

/**
 * 测试解析type为M的json,修改thread_sort_index
 */
TEST_F(EventParserTest, TestMParseModifyThreadSortIndex)
{
    std::string jsonContent = "[{\"name\": \"thread_sort_index\", \"pid\": 2094647552, \"tid\": 8, \"args\": "
        "{\"sort_index\": \"333\"}, \"ph\": "
        "\"M\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    const uint64_t expectSize = 1;
    const uint64_t expectIndex = 333;
    const uint64_t first = 0;
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .ExcuteQuery(dbPtr, threadPOS);
    EXPECT_EQ(threadPOS.size(), expectSize);
    EXPECT_EQ(1, threadPOS[first].trackId);
    EXPECT_EQ(expectIndex, threadPOS[first].threadSortIndex);
}

/**
 * 测试解析type为M的json,m数据内容无效
 */
TEST_F(EventParserTest, TestMParseModifyWithWrongContent)
{
    std::string jsonContent =
        "[{\"name\": \"bbbbbb\", \"pid\": 2094647552, \"tid\": 8, \"args\": {\"sort_index\": \"333\"}, \"ph\": "
        "\"M\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    EXPECT_NO_THROW(eventParserMock.Parse(startPosition, endPosition));
}

/**
 * 测试解析type为SM的json,修改thread_sort_index
 */
TEST_F(EventParserTest, TestSMParseModifyThreadSortIndex)
{
    std::string jsonContent = "[{\"name\": \"thread_sort_index\", \"pid\": 2094647552, \"tid\": 8, \"args\": "
        "{\"sort_index\": \"333\"}, \"ph\": "
        "\"M\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    const uint64_t expectSize = 1;
    const uint64_t expectIndex = 333;
    const uint64_t first = 0;
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .ExcuteQuery(dbPtr, threadPOS);
    EXPECT_EQ(threadPOS.size(), expectSize);
    EXPECT_EQ(1, threadPOS[first].trackId);
    EXPECT_EQ(expectIndex, threadPOS[first].threadSortIndex);
}


/**
 * 测试解析type为SC的json,修改thread_sort_index
 */
TEST_F(EventParserTest, TestSCParseModifyThreadSortIndex)
{
    std::string jsonContent =
        "[{\"name\": \"bbbbbb\", \"pid\": 2094647552, \"tid\": 8, \"args\": {\"sort_index\": \"333\"}, \"ph\": "
        "\"C\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    EXPECT_NO_THROW(eventParserMock.Parse(startPosition, endPosition));
}

/**
 * 测试系统调优解析type为s,f,t的json
 */
TEST_F(EventParserTest, TestPhIssParseModifyThreadSortIndex)
{
    std::string jsonContent = "[{\"name\": \"HostToDevice51539607551\", \"ph\": \"s\", \"cat\": \"HostToDevice\", "
        "\"id\": \"51539607551\", \"pid\": 2374902, \"tid\": 27165, \"ts\": 1700121143782025.0},{\"name\": "
        "\"HostToDevice51539607551\", \"ph\": \"f\", \"id\": \"51539607551\", \"ts\": 1700121143782435.2, \"cat\": "
        "\"HostToDevice\", \"pid\": 237491400, \"tid\": 0, \"bp\": \"e\"},{\"name\": \"HostToDevice51539607551\", "
        "\"ph\": \"t\", \"id\": \"51539607551\", \"ts\": 1700121143782435.2, \"cat\": \"HostToDevice\", \"pid\": "
        "237491400, \"tid\": 0, \"bp\": \"e\"}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.Parse(startPosition, endPosition);
    const uint64_t expectSize = 3;
    const uint64_t first = 0;
    const uint64_t second = 1;
    const uint64_t third = 2;
    const uint64_t expectTime = 1700121143782435200;
    Dic::Module::Timeline::FlowTable flowTable;
    std::vector<FlowPO> flowPOS;
    flowTable.Select(FlowColumn::ID, FlowColumn::TRACK_ID)
        .Select(FlowColumn::TIMESTAMP, FlowColumn::NAME)
        .Select(FlowColumn::TYPE, FlowColumn::CAT)
        .Select(FlowColumn::FLOW_ID)
        .OrderBy(FlowColumn::ID, TableOrder::ASC)
        .ExcuteQuery(dbPtr, flowPOS);
    EXPECT_EQ(flowPOS.size(), expectSize);
    EXPECT_EQ(1, flowPOS[first].id);
    EXPECT_EQ(1, flowPOS[first].trackId);
    EXPECT_EQ(expectTime, flowPOS[second].timestamp);
    EXPECT_EQ("HostToDevice51539607551", flowPOS[second].name);
    EXPECT_EQ("t", flowPOS[third].type);
    EXPECT_EQ("HostToDevice", flowPOS[third].cat);
    EXPECT_EQ("51539607551", flowPOS[third].flowId);
}

/**
 * 测试算子调优解析type为s,f的json
 */
TEST_F(EventParserTest, TestParseSimulationFlowJson)
{
    std::string jsonContent =
        "[{\"cat\":\"MTE2ToVECTOR\",\"id\":1,\"name\":\"flow\",\"ph\":\"s\",\"pid\":\"core0.veccore0\",\"tid\":"
        "\"MTE2\",\"ts\":1.0064865350723267},{\"cat\":\"MTE2ToVECTOR\",\"id\":2,\"name\":\"flow\",\"ph\":\"t\",\"pid\":"
        "\"core0.veccore0\",\"tid\":\"VECTOR\",\"ts\":1.00702702999115}]";
    sqlite3 *dbPtr = nullptr;
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    const uint64_t expectSize = 2;
    const uint64_t first = 0;
    const uint64_t second = 1;
    const uint64_t expectTime = 1006;
    Dic::Module::Timeline::FlowTable flowTable;
    std::vector<FlowPO> flowPOS;
    flowTable.Select(FlowColumn::ID, FlowColumn::TRACK_ID)
        .Select(FlowColumn::TIMESTAMP, FlowColumn::NAME)
        .Select(FlowColumn::TYPE, FlowColumn::CAT)
        .Select(FlowColumn::FLOW_ID)
        .OrderBy(FlowColumn::ID, TableOrder::ASC)
        .ExcuteQuery(dbPtr, flowPOS);
    EXPECT_EQ(flowPOS.size(), expectSize);
    EXPECT_EQ(1, flowPOS[first].id);
    EXPECT_EQ(1, flowPOS[first].trackId);
    EXPECT_EQ(expectTime, flowPOS[first].timestamp);
    EXPECT_EQ("flow", flowPOS[second].name);
    EXPECT_EQ("t", flowPOS[second].type);
    EXPECT_EQ("MTE2ToVECTOR", flowPOS[second].cat);
    EXPECT_EQ("2", flowPOS[second].flowId);
}

/**
 * 算子调优测试解析type为X的json
 */
TEST_F(EventParserTest, TestSimulationSliceParse)
{
    std::string jsonContent = "[{\"args\":{\"code\":\"/home/yangyidiao/sample/samples_new/samples-master/operator/"
        "MatMulLeakyReluCustomSample/FrameworkLaunch/MatmulLeakyReluCustom/build_out/op_kernel/binary/ascend910b/"
        "kernel_meta_MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7/kernel_meta/"
        "MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7_16186_kernel.cpp:23\",\"detail\":\"XD:X29=0x107f80,IMM:"
        "0x10,UIMM:0x1,\",\"pc_addr\":\"0x1269f004\"},\"cname\":\"startup\",\"dur\":0.0010000000474974513,\"name\":"
        "\"MOVK\",\"ph\":\"X\",\"pid\":\"core0.cubecore0\",\"tid\":\"SCALAR\",\"ts\":2.9260001182556152}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CNAME)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    const uint64_t exceptTime = 2926;
    const uint64_t exceptDuration = 1;
    const uint64_t exceptTrackId = 1;
    const std::string name = "MOVK";
    EXPECT_EQ(slicePOs.size(), expectSize);
    EXPECT_EQ(slicePOs[first].timestamp, exceptTime);
    EXPECT_EQ(slicePOs[first].duration, exceptDuration);
    EXPECT_EQ(slicePOs[first].name, name);
    EXPECT_EQ(slicePOs[first].trackId, exceptTrackId);
    EXPECT_EQ(slicePOs[first].args.empty(), false);
    EXPECT_EQ(slicePOs[first].cname, "startup");
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .Select(ThreadColumn::THREAD_SORT_INDEX)
        .ExcuteQuery(dbPtr, threadPOS);
    EXPECT_EQ(threadPOS.size(), expectSize);
    EXPECT_EQ(1, threadPOS[first].trackId);
    EXPECT_EQ("SCALAR", threadPOS[first].threadName);
    EXPECT_EQ("core0.cubecore0", threadPOS[first].pid);
    EXPECT_EQ(1, threadPOS[first].threadSortIndex);
}

/**
 * 算子调优测试解析type为X的json,没有tid和pid
 */
TEST_F(EventParserTest, TestSimulationSliceParseWithOutTidAndPid)
{
    std::string jsonContent = "[{\"args\":{\"code\":\"/home/yangyidiao/sample/samples_new/samples-master/operator/"
        "MatMulLeakyReluCustomSample/FrameworkLaunch/MatmulLeakyReluCustom/build_out/op_kernel/binary/ascend910b/"
        "kernel_meta_MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7/kernel_meta/"
        "MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7_16186_kernel.cpp:23\",\"detail\":\"XD:X29=0x107f80,IMM:"
        "0x10,UIMM:0x1,\",\"pc_addr\":\"0x1269f004\"},\"cname\":\"startup\",\"dur\":0.0010000000474974513,\"name\":"
        "\"MOVK\",\"ph\":\"X\",\"ts\":2.9260001182556152}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CNAME)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 0;
    EXPECT_EQ(slicePOs.size(), expectSize);
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .Select(ThreadColumn::THREAD_SORT_INDEX)
        .ExcuteQuery(dbPtr, threadPOS);
    EXPECT_EQ(threadPOS.size(), expectSize);
}

/**
 * 算子调优测试解析type只有为B的WAIT_FLAG的json
 */
TEST_F(EventParserTest, TestSimulationPhIsBParse)
{
    std::string jsonContent =
        "[{\"args\":{\"code\":\"/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "kernel_event.h:778\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/interface/"
        "kernel_common.h:159\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "dav_c220/kfc/kfc_comm.h:274\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/lib/"
        "matmul_intf.h:142\\n/home/yangyidiao/sample/samples_new/samples-master/operator/MatMulLeakyReluCustomSample/"
        "FrameworkLaunch/MatmulLeakyReluCustom/build_out/op_kernel/binary/ascend910b/"
        "kernel_meta_MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7/kernel_meta/"
        "MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7_16186_kernel.cpp:39\",\"detail\":\"PIPE:MTE2,"
        "TRIGGERPIPE:MTE3,FLAGID:0,\",\"pc_addr\":\"0x1269f0ac\"},\"cname\":\"rail_response\",\"id\":0,\"name\":\"WAIT_"
        "FLAG\",\"ph\":\"B\",\"pid\":\"core0.cubecore0\",\"tid\":\"MTE3\",\"ts\":3.1454052925109863}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CNAME)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 0;
    EXPECT_EQ(slicePOs.size(), expectSize);
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .Select(ThreadColumn::THREAD_SORT_INDEX)
        .ExcuteQuery(dbPtr, threadPOS);
    EXPECT_EQ(threadPOS.size(), expectSize);
}

/**
 * 算子调优测试解析type为B和E都存在的WAIT_FLAG的json
 */
TEST_F(EventParserTest, TestSimulationPhIsBAndEParse)
{
    std::string jsonContent =
        "[{\"args\":{\"code\":\"/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "kernel_event.h:778\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/interface/"
        "kernel_common.h:159\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "dav_c220/kfc/kfc_comm.h:274\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/lib/"
        "matmul_intf.h:142\\n/home/yangyidiao/sample/samples_new/samples-master/operator/MatMulLeakyReluCustomSample/"
        "FrameworkLaunch/MatmulLeakyReluCustom/build_out/op_kernel/binary/ascend910b/"
        "kernel_meta_MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7/kernel_meta/"
        "MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7_16186_kernel.cpp:39\",\"detail\":\"PIPE:MTE2,"
        "TRIGGERPIPE:MTE3,FLAGID:0,\",\"pc_addr\":\"0x1269f0ac\"},\"cname\":\"rail_response\",\"group_id\":\"groupA\","
        "\"id\":5,\"name\":\"WAIT_"
        "FLAG\",\"ph\":\"B\",\"pid\":\"core0.cubecore0\",\"tid\":\"MTE3\",\"ts\":3.1454052925109863},{\"args\":{},"
        "\"cname\":\"rail_response\",\"id\":5,\"name\":\"WAIT_FLAG\",\"ph\":\"E\",\"pid\":\"core0.cubecore0\",\"tid\":"
        "\"MTE3\",\"ts\":3.184324264526367}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CNAME)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .Select(SliceColumn::FLAGID, SliceColumn::GROUPID)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    const uint64_t exceptTime = 3145;
    const uint64_t exceptDuration = 39;
    const uint64_t exceptTrackId = 1;
    const std::string name = "WAIT_FLAG";
    EXPECT_EQ(slicePOs.size(), expectSize);
    EXPECT_EQ(slicePOs[first].timestamp, exceptTime);
    EXPECT_EQ(slicePOs[first].duration, exceptDuration);
    EXPECT_EQ(slicePOs[first].name, name);
    EXPECT_EQ(slicePOs[first].trackId, exceptTrackId);
    EXPECT_EQ(slicePOs[first].endTime, exceptTime + exceptDuration);
    EXPECT_EQ(slicePOs[first].args.empty(), false);
    EXPECT_EQ(slicePOs[first].cname, "rail_response");
    EXPECT_EQ(slicePOs[first].flagId, "5");
    EXPECT_EQ(slicePOs[first].groupId, "groupA");
}

/**
 * 算子调优测试解析type为B和E都存在的wait_event的json
 */
TEST_F(EventParserTest, TestWait_eventSimulationPhIsBAndEParse)
{
    std::string jsonContent =
        "[{\"args\":{\"code\":\"/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "kernel_event.h:778\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/interface/"
        "kernel_common.h:159\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "dav_c220/kfc/kfc_comm.h:274\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/lib/"
        "matmul_intf.h:142\\n/home/yangyidiao/sample/samples_new/samples-master/operator/MatMulLeakyReluCustomSample/"
        "FrameworkLaunch/MatmulLeakyReluCustom/build_out/op_kernel/binary/ascend910b/"
        "kernel_meta_MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7/kernel_meta/"
        "MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7_16186_kernel.cpp:39\",\"detail\":\"PIPE:MTE2,"
        "TRIGGERPIPE:MTE3,FLAGID:0,\",\"pc_addr\":\"0x1269f0ac\"},\"cname\":\"rail_response\",\"id\":5,\"name\":\"wait_"
        "event\",\"ph\":\"B\",\"pid\":\"core0.cubecore0\",\"tid\":\"MTE3\",\"ts\":3.1454052925109863},{\"args\":{},"
        "\"cname\":\"rail_response\",\"id\":5,\"name\":\"wait_event\",\"ph\":\"E\",\"pid\":\"core0.cubecore0\",\"tid\":"
        "\"MTE3\",\"ts\":3.184324264526367}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CNAME)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .Select(SliceColumn::FLAGID)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    const uint64_t exceptTime = 3145;
    const uint64_t exceptDuration = 39;
    const uint64_t exceptTrackId = 1;
    const std::string name = "wait_event";
    EXPECT_EQ(slicePOs.size(), expectSize);
    EXPECT_EQ(slicePOs[first].timestamp, exceptTime);
    EXPECT_EQ(slicePOs[first].duration, exceptDuration);
    EXPECT_EQ(slicePOs[first].name, name);
    EXPECT_EQ(slicePOs[first].trackId, exceptTrackId);
    EXPECT_EQ(slicePOs[first].endTime, exceptTime + exceptDuration);
    EXPECT_EQ(slicePOs[first].args.empty(), false);
    EXPECT_EQ(slicePOs[first].cname, "rail_response");
    EXPECT_EQ(slicePOs[first].flagId, "5");
}

/**
 * 算子调优测试解析type为B和E都存在的SET_FLAG的json
 */
TEST_F(EventParserTest, TestSET_FLAGSimulationPhIsBAndEParse)
{
    std::string jsonContent =
        "[{\"args\":{\"code\":\"/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "kernel_event.h:778\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/interface/"
        "kernel_common.h:159\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "dav_c220/kfc/kfc_comm.h:274\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/lib/"
        "matmul_intf.h:142\\n/home/yangyidiao/sample/samples_new/samples-master/operator/MatMulLeakyReluCustomSample/"
        "FrameworkLaunch/MatmulLeakyReluCustom/build_out/op_kernel/binary/ascend910b/"
        "kernel_meta_MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7/kernel_meta/"
        "MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7_16186_kernel.cpp:39\",\"detail\":\"PIPE:MTE2,"
        "TRIGGERPIPE:MTE3,FLAGID:0,\",\"pc_addr\":\"0x1269f0ac\"},\"cname\":\"rail_response\",\"id\":5,\"name\":\"SET_"
        "FLAG\",\"ph\":\"B\",\"pid\":\"core0.cubecore0\",\"tid\":\"MTE3\",\"ts\":3.1454052925109863},{\"args\":{},"
        "\"cname\":\"rail_response\",\"id\":5,\"name\":\"SET_FLAG\",\"ph\":\"E\",\"pid\":\"core0.cubecore0\",\"tid\":"
        "\"MTE3\",\"ts\":3.184324264526367}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CNAME)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .Select(SliceColumn::FLAGID)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    const uint64_t exceptTime = 3145;
    const uint64_t exceptDuration = 39;
    const uint64_t exceptTrackId = 1;
    const std::string name = "SET_FLAG";
    EXPECT_EQ(slicePOs.size(), expectSize);
    EXPECT_EQ(slicePOs[first].timestamp, exceptTime);
    EXPECT_EQ(slicePOs[first].duration, exceptDuration);
    EXPECT_EQ(slicePOs[first].name, name);
    EXPECT_EQ(slicePOs[first].trackId, exceptTrackId);
    EXPECT_EQ(slicePOs[first].endTime, exceptTime + exceptDuration);
    EXPECT_EQ(slicePOs[first].args.empty(), false);
    EXPECT_EQ(slicePOs[first].cname, "rail_response");
    EXPECT_EQ(slicePOs[first].flagId, "5");
}

/**
 * 算子调优测试解析type为B和E都存在的SET_FLAG的json,且E在前，B在后
 */
TEST_F(EventParserTest, TestSET_FLAGSimulationPhIsBAndEAndEIsBeforeParse)
{
    std::string jsonContent =
        "[{\"args\":{},\"cname\":\"rail_response\",\"id\":5,\"name\":\"SET_FLAG\",\"ph\":\"E\",\"pid\":\"core0."
        "cubecore0\",\"tid\":\"MTE3\",\"ts\":3.184324264526367},{\"args\":{\"code\":\"/home/yangyidiao/Ascend64/"
        "ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "kernel_event.h:778\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/interface/"
        "kernel_common.h:159\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "dav_c220/kfc/kfc_comm.h:274\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/lib/"
        "matmul_intf.h:142\\n/home/yangyidiao/sample/samples_new/samples-master/operator/MatMulLeakyReluCustomSample/"
        "FrameworkLaunch/MatmulLeakyReluCustom/build_out/op_kernel/binary/ascend910b/"
        "kernel_meta_MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7/kernel_meta/"
        "MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7_16186_kernel.cpp:39\",\"detail\":\"PIPE:MTE2,"
        "TRIGGERPIPE:MTE3,FLAGID:0,\",\"pc_addr\":\"0x1269f0ac\"},\"cname\":\"rail_response\",\"id\":5,\"name\":\"SET_"
        "FLAG\",\"ph\":\"B\",\"pid\":\"core0.cubecore0\",\"tid\":\"MTE3\",\"ts\":3.1454052925109863}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CNAME)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .Select(SliceColumn::FLAGID)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    const uint64_t exceptTime = 3145;
    const uint64_t exceptDuration = 39;
    const uint64_t exceptTrackId = 1;
    const std::string name = "SET_FLAG";
    EXPECT_EQ(slicePOs.size(), expectSize);
    EXPECT_EQ(slicePOs[first].timestamp, exceptTime);
    EXPECT_EQ(slicePOs[first].duration, exceptDuration);
    EXPECT_EQ(slicePOs[first].name, name);
    EXPECT_EQ(slicePOs[first].trackId, exceptTrackId);
    EXPECT_EQ(slicePOs[first].endTime, exceptTime + exceptDuration);
    EXPECT_EQ(slicePOs[first].args.empty(), false);
    EXPECT_EQ(slicePOs[first].cname, "rail_response");
    EXPECT_EQ(slicePOs[first].flagId, "5");
}

/**
 * 算子调优测试解析type为B和E都存在的WAIT_FLAG的json,且E在前，B在后
 */
TEST_F(EventParserTest, TestWAIT_FLAGSimulationPhIsBAndEAndEIsBeforeParse)
{
    std::string jsonContent =
        "[{\"args\":{},\"cname\":\"rail_response\",\"id\":5,\"name\":\"WAIT_FLAG\",\"ph\":\"E\",\"pid\":\"core0."
        "cubecore0\",\"tid\":\"MTE3\",\"ts\":3.184324264526367},{\"args\":{\"code\":\"/home/yangyidiao/Ascend64/"
        "ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "kernel_event.h:778\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/interface/"
        "kernel_common.h:159\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "dav_c220/kfc/kfc_comm.h:274\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/lib/"
        "matmul_intf.h:142\\n/home/yangyidiao/sample/samples_new/samples-master/operator/MatMulLeakyReluCustomSample/"
        "FrameworkLaunch/MatmulLeakyReluCustom/build_out/op_kernel/binary/ascend910b/"
        "kernel_meta_MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7/kernel_meta/"
        "MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7_16186_kernel.cpp:39\",\"detail\":\"PIPE:MTE2,"
        "TRIGGERPIPE:MTE3,FLAGID:0,\",\"pc_addr\":\"0x1269f0ac\"},\"cname\":\"rail_response\",\"id\":5,\"name\":\"WAIT_"
        "FLAG\",\"ph\":\"B\",\"pid\":\"core0.cubecore0\",\"tid\":\"MTE3\",\"ts\":3.1454052925109863}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CNAME)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .Select(SliceColumn::FLAGID)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 1;
    const uint64_t first = 0;
    const uint64_t exceptTime = 3145;
    const uint64_t exceptDuration = 39;
    const uint64_t exceptTrackId = 1;
    const std::string name = "WAIT_FLAG";
    EXPECT_EQ(slicePOs.size(), expectSize);
    EXPECT_EQ(slicePOs[first].timestamp, exceptTime);
    EXPECT_EQ(slicePOs[first].duration, exceptDuration);
    EXPECT_EQ(slicePOs[first].name, name);
    EXPECT_EQ(slicePOs[first].trackId, exceptTrackId);
    EXPECT_EQ(slicePOs[first].endTime, exceptTime + exceptDuration);
    EXPECT_EQ(slicePOs[first].args.empty(), false);
    EXPECT_EQ(slicePOs[first].cname, "rail_response");
    EXPECT_EQ(slicePOs[first].flagId, "5");
}

/**
 * 算子调优测试解析type只有为B的SET_FLAG的json
 */
TEST_F(EventParserTest, TestSET_FLAGSimulationPhIsBParse)
{
    std::string jsonContent =
        "[{\"args\":{\"code\":\"/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "kernel_event.h:778\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/interface/"
        "kernel_common.h:159\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/impl/"
        "dav_c220/kfc/kfc_comm.h:274\\n/home/yangyidiao/Ascend64/ascend-toolkit/8.0.T13/x86_64-linux/tikcpp/tikcfw/lib/"
        "matmul_intf.h:142\\n/home/yangyidiao/sample/samples_new/samples-master/operator/MatMulLeakyReluCustomSample/"
        "FrameworkLaunch/MatmulLeakyReluCustom/build_out/op_kernel/binary/ascend910b/"
        "kernel_meta_MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7/kernel_meta/"
        "MatmulLeakyreluCustom_0a4ecf73240c8db04ac5059db7c787a7_16186_kernel.cpp:39\",\"detail\":\"PIPE:MTE2,"
        "TRIGGERPIPE:MTE3,FLAGID:0,\",\"pc_addr\":\"0x1269f0ac\"},\"cname\":\"rail_response\",\"id\":0,\"name\":\"SET_"
        "FLAG\",\"ph\":\"B\",\"pid\":\"core0.cubecore0\",\"tid\":\"MTE3\",\"ts\":3.1454052925109863}]";
    ParserStatusManager::Instance().SetParserStatus(fileId, ParserStatus::RUNNING);
    EXPECT_CALL(*mockFileReader, ReadJsonArray(filePath, startPosition, endPosition)).WillOnce(Return(jsonContent));
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    mockDatabase->SetDbPtr(dbPtr);
    mockDatabase->CreateTable();
    EventParserMock eventParserMock(filePath, fileId, mockDatabase);
    eventParserMock.SetFileReaderAndDatabase(std::move(mockFileReader));
    eventParserMock.SetSimulationStatus(true);
    eventParserMock.Parse(startPosition, endPosition);
    Dic::Module::Timeline::SliceTable sliceTable;
    std::vector<SlicePO> slicePOs;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CNAME)
        .Select(SliceColumn::ENDTIME, SliceColumn::ARGS)
        .ExcuteQuery(dbPtr, slicePOs);
    const uint64_t expectSize = 0;
    EXPECT_EQ(slicePOs.size(), expectSize);
    Dic::Module::Timeline::ThreadTable threadTable;
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .Select(ThreadColumn::THREAD_SORT_INDEX)
        .ExcuteQuery(dbPtr, threadPOS);
    EXPECT_EQ(threadPOS.size(), expectSize);
}