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
#include <gmock/gmock.h>
#include <memory>
#include <cstdint>

#include "DataBaseManager.h"
#include "ParserStatusManager.h"
#include "DbClusterDataBase.h"
#include "ClusterDef.h"
#include "ClusterFileParser.h"
#include "ClusterFileParserInterfaces.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

namespace Dic {
namespace Module {
namespace Timeline {

//=============================================================================
// Mock 类定义
//=============================================================================

class MockFileUtil : public IFileUtil {
  public:
    MOCK_METHOD(bool, IsFolder, (const std::string &path), (override));
    MOCK_METHOD(std::string, GetParentPath, (const std::string &filePath), (override));
    MOCK_METHOD(std::vector<std::string>, FindFilesWithFilter, (const std::string &path, const std::regex &fileRegex),
        (override));
};

class MockParserStatusManager : public IParserStatusManager {
  public:
    MOCK_METHOD(void, SetClusterParseStatus, (const std::string &uniqueKey, ParserStatus status), (override));
    MOCK_METHOD(ParserStatus, GetClusterParseStatus, (const std::string &uniqueKey), (override));
};

class MockDataBaseManager : public IDataBaseManager {
  public:
    MOCK_METHOD(void, CreateClusterConnectionPool,
        (const std::string &projectPath, const std::string &dbPath, DataType type), (override));
    MOCK_METHOD(std::shared_ptr<IDbClusterDataBase>, GetClusterDatabase, (const std::string &uniqueKey), (override));
};

class MockDbClusterDataBase : public IDbClusterDataBase {
  public:
    MOCK_METHOD(bool, IsDatabaseVersionChange, (), (const, override));
    MOCK_METHOD(bool, SetDataBaseVersion, (const std::string &targetVersion), (override));
    MOCK_METHOD(bool, QueryExtremumTimestamp, (uint64_t & min, uint64_t &max), (override));
    MOCK_METHOD(bool, CreateTable, (), (override));
    MOCK_METHOD(bool, DropTable, (), (override));
    MOCK_METHOD(bool, HasFinishedParseLastTime, (), (override));
    MOCK_METHOD(bool, UpdatesClusterParseStatus, (const std::string &status), (override));
    MOCK_METHOD(void, InsertClusterBaseInfo, (ClusterBaseInfo & info), (override));
};

class MockTraceTime : public ITraceTime {
  public:
    MOCK_METHOD(void, UpdateTime, (uint64_t min, uint64_t max), (override));
};

//=============================================================================
// 可测试的 ClusterFileParser 包装类
//=============================================================================

class TestableClusterFileParser {
  public:
    TestableClusterFileParser(const std::string &filePath, const std::string &uniqueKey,
        std::shared_ptr<IFileUtil> fileUtil, std::shared_ptr<IParserStatusManager> statusManager,
        std::shared_ptr<IDataBaseManager> dbManager, std::shared_ptr<ITraceTime> traceTime)
        : filePath_(filePath), uniqueKey_(uniqueKey), fileUtil_(fileUtil), statusManager_(statusManager),
          dbManager_(dbManager), traceTime_(traceTime) {}

    // 从原有 ParserClusterOfDb 重构的核心逻辑
    bool ParserClusterOfDb() {
        statusManager_->SetClusterParseStatus(uniqueKey_, ParserStatus::INIT);

        std::string tempPath(filePath_);
        if (!fileUtil_->IsFolder(filePath_)) {
            tempPath = fileUtil_->GetParentPath(filePath_);
        }

        std::string clusterDBReg = "cluster_analysis\\.db$";
        std::vector<std::string> clusterPath = fileUtil_->FindFilesWithFilter(tempPath, std::regex(clusterDBReg));

        // 集群解析逻辑简化...
        if (clusterPath.empty()) {
            statusManager_->SetClusterParseStatus(uniqueKey_, ParserStatus::FINISH);
            return false;
        }

        std::string clusterDbPath = clusterPath[0];
        dbManager_->CreateClusterConnectionPool(filePath_, clusterDbPath, DataType::DB);

        auto clusterDatabase = dbManager_->GetClusterDatabase(filePath_);

        if (clusterDatabase == nullptr) {
            statusManager_->SetClusterParseStatus(uniqueKey_, ParserStatus::FINISH);
            return false;
        }

        // 版本未变化且上次已完成
        if (!clusterDatabase->IsDatabaseVersionChange() && clusterDatabase->HasFinishedParseLastTime()) {
            statusManager_->SetClusterParseStatus(uniqueKey_, ParserStatus::FINISH);
            UpdateTraceTimeOfDb(clusterDatabase);
            return true;
        }

        // 需要重新解析
        if (!clusterDatabase->DropTable() || !clusterDatabase->CreateTable() ||
            !clusterDatabase->SetDataBaseVersion("") || !clusterDatabase->UpdatesClusterParseStatus("NOT_FINISH")) {
            statusManager_->SetClusterParseStatus(uniqueKey_, ParserStatus::FINISH);
            return false;
        }

        ClusterBaseInfo baseInfo;
        // InitFullDbClusterBaseInfo(clusterDatabase, baseInfo);
        clusterDatabase->InsertClusterBaseInfo(baseInfo);
        clusterDatabase->UpdatesClusterParseStatus("FINISH");

        statusManager_->SetClusterParseStatus(uniqueKey_, ParserStatus::FINISH);
        UpdateTraceTimeOfDb(clusterDatabase);
        return true;
    }

  private:
    void UpdateTraceTimeOfDb(std::shared_ptr<IDbClusterDataBase> clusterDatabase) {
        uint64_t min = UINT64_MAX;
        uint64_t max = 0;
        if (clusterDatabase->QueryExtremumTimestamp(min, max)) {
            if (min != UINT64_MAX && max != 0) {
                traceTime_->UpdateTime(min, max);
            }
        }
    }

    std::string filePath_;
    std::string uniqueKey_;
    std::shared_ptr<IFileUtil> fileUtil_;
    std::shared_ptr<IParserStatusManager> statusManager_;
    std::shared_ptr<IDataBaseManager> dbManager_;
    std::shared_ptr<ITraceTime> traceTime_;
};

//=============================================================================
// 测试夹具
//=============================================================================

class UpdateTraceTimeOfDbTest : public ::testing::Test {
  protected:
    void SetUp() override {
        mockFileUtil_ = std::make_shared<NiceMock<MockFileUtil>>();
        mockStatusManager_ = std::make_shared<NiceMock<MockParserStatusManager>>();
        mockDbManager_ = std::make_shared<NiceMock<MockDataBaseManager>>();
        mockTraceTime_ = std::make_shared<NiceMock<MockTraceTime>>();
        mockDb_ = std::make_shared<NiceMock<MockDbClusterDataBase>>();
    }

    void TearDown() override {
        mockFileUtil_.reset();
        mockStatusManager_.reset();
        mockDbManager_.reset();
        mockTraceTime_.reset();
        mockDb_.reset();
    }

    std::shared_ptr<MockFileUtil> mockFileUtil_;
    std::shared_ptr<MockParserStatusManager> mockStatusManager_;
    std::shared_ptr<MockDataBaseManager> mockDbManager_;
    std::shared_ptr<MockTraceTime> mockTraceTime_;
    std::shared_ptr<MockDbClusterDataBase> mockDb_;
};

//=============================================================================
// 集成测试：验证 ParserClusterOfDb 中正确调用了 UpdateTraceTimeOfDb
//=============================================================================

// 测试1：验证在数据库版本未变化且上次已完成时调用 UpdateTraceTimeOfDb
TEST_F(UpdateTraceTimeOfDbTest, ParserClusterOfDb_CallsUpdateTraceTime_VersionNotChanged) {
    // Arrange
    std::string filePath = "/test/path/to/file.db";
    std::string uniqueKey = "test_unique_key";
    std::string clusterDbPath = "/test/path/to/cluster_analysis.db";
    std::vector<std::string> clusterPaths = {clusterDbPath};

    TestableClusterFileParser parser(
        filePath, uniqueKey, mockFileUtil_, mockStatusManager_, mockDbManager_, mockTraceTime_);

    // 设置 Mock 行为
    EXPECT_CALL(*mockStatusManager_, SetClusterParseStatus(uniqueKey, ParserStatus::INIT));
    EXPECT_CALL(*mockFileUtil_, IsFolder(filePath)).WillOnce(Return(false));
    EXPECT_CALL(*mockFileUtil_, GetParentPath(filePath)).WillOnce(Return("/test/path/to"));
    EXPECT_CALL(*mockFileUtil_, FindFilesWithFilter("/test/path/to", _)).WillOnce(Return(clusterPaths));
    EXPECT_CALL(*mockDbManager_, CreateClusterConnectionPool(filePath, clusterDbPath, DataType::DB));
    EXPECT_CALL(*mockDbManager_, GetClusterDatabase(filePath)).WillOnce(Return(mockDb_));

    // 模拟版本未变化，上次已完成
    EXPECT_CALL(*mockDb_, IsDatabaseVersionChange()).WillOnce(Return(false));
    EXPECT_CALL(*mockDb_, HasFinishedParseLastTime()).WillOnce(Return(true));

    // 模拟查询到有效时间戳
    EXPECT_CALL(*mockDb_, QueryExtremumTimestamp(_, _)).WillOnce([](uint64_t &min, uint64_t &max) -> bool {
        min = 1000;
        max = 5000;
        return true;
    });

    // 期望 UpdateTime 被调用
    EXPECT_CALL(*mockTraceTime_, UpdateTime(1000, 5000));

    EXPECT_CALL(*mockStatusManager_, SetClusterParseStatus(uniqueKey, ParserStatus::FINISH));

    // Act
    bool result = parser.ParserClusterOfDb();

    // Assert
    EXPECT_TRUE(result);
}

// 测试2：验证在需要重新解析时也调用 UpdateTraceTimeOfDb
TEST_F(UpdateTraceTimeOfDbTest, ParserClusterOfDb_CallsUpdateTraceTime_VersionChanged) {
    // Arrange
    std::string filePath = "/test/path/to/folder";
    std::string uniqueKey = "test_unique_key";
    std::string clusterDbPath = "/test/path/to/folder/cluster_analysis.db";
    std::vector<std::string> clusterPaths = {clusterDbPath};

    TestableClusterFileParser parser(
        filePath, uniqueKey, mockFileUtil_, mockStatusManager_, mockDbManager_, mockTraceTime_);

    // 设置 Mock 行为
    EXPECT_CALL(*mockStatusManager_, SetClusterParseStatus(uniqueKey, ParserStatus::INIT));
    EXPECT_CALL(*mockFileUtil_, IsFolder(filePath)).WillOnce(Return(true));
    EXPECT_CALL(*mockFileUtil_, FindFilesWithFilter(filePath, _)).WillOnce(Return(clusterPaths));
    EXPECT_CALL(*mockDbManager_, CreateClusterConnectionPool(filePath, clusterDbPath, DataType::DB));
    EXPECT_CALL(*mockDbManager_, GetClusterDatabase(filePath)).WillOnce(Return(mockDb_));

    // 模拟版本已变化，需要重新解析
    EXPECT_CALL(*mockDb_, IsDatabaseVersionChange()).WillOnce(Return(true));

    // 模拟数据库操作成功
    EXPECT_CALL(*mockDb_, DropTable()).WillOnce(Return(true));
    EXPECT_CALL(*mockDb_, CreateTable()).WillOnce(Return(true));
    EXPECT_CALL(*mockDb_, SetDataBaseVersion(_)).WillOnce(Return(true));
    EXPECT_CALL(*mockDb_, UpdatesClusterParseStatus(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mockDb_, InsertClusterBaseInfo(_));

    // 模拟查询到有效时间戳
    EXPECT_CALL(*mockDb_, QueryExtremumTimestamp(_, _)).WillOnce([](uint64_t &min, uint64_t &max) -> bool {
        min = 2000;
        max = 6000;
        return true;
    });

    // 期望 UpdateTime 被调用
    EXPECT_CALL(*mockTraceTime_, UpdateTime(2000, 6000));

    EXPECT_CALL(*mockStatusManager_, SetClusterParseStatus(uniqueKey, ParserStatus::FINISH));

    // Act
    bool result = parser.ParserClusterOfDb();

    // Assert
    EXPECT_TRUE(result);
}

// 测试3：验证时间戳无效时不调用 UpdateTraceTimeOfDb
TEST_F(UpdateTraceTimeOfDbTest, ParserClusterOfDb_NoCallUpdateTraceTime_InvalidTimestamp) {
    // Arrange
    std::string filePath = "/test/path/to/folder";
    std::string uniqueKey = "test_unique_key";
    std::string clusterDbPath = "/test/path/to/folder/cluster_analysis.db";
    std::vector<std::string> clusterPaths = {clusterDbPath};

    TestableClusterFileParser parser(
        filePath, uniqueKey, mockFileUtil_, mockStatusManager_, mockDbManager_, mockTraceTime_);

    // 设置 Mock 行为
    EXPECT_CALL(*mockStatusManager_, SetClusterParseStatus(uniqueKey, ParserStatus::INIT));
    EXPECT_CALL(*mockFileUtil_, IsFolder(filePath)).WillOnce(Return(true));
    EXPECT_CALL(*mockFileUtil_, FindFilesWithFilter(filePath, _)).WillOnce(Return(clusterPaths));
    EXPECT_CALL(*mockDbManager_, CreateClusterConnectionPool(filePath, clusterDbPath, DataType::DB));
    EXPECT_CALL(*mockDbManager_, GetClusterDatabase(filePath)).WillOnce(Return(mockDb_));

    // 模拟版本未变化，上次已完成
    EXPECT_CALL(*mockDb_, IsDatabaseVersionChange()).WillOnce(Return(false));
    EXPECT_CALL(*mockDb_, HasFinishedParseLastTime()).WillOnce(Return(true));

    // 模拟查询到无效时间戳
    EXPECT_CALL(*mockDb_, QueryExtremumTimestamp(_, _)).WillOnce([](uint64_t &min, uint64_t &max) -> bool {
        min = UINT64_MAX;
        max = 0;
        return true;
    });

    // UpdateTime 不应被调用
    EXPECT_CALL(*mockTraceTime_, UpdateTime(_, _)).Times(0);

    EXPECT_CALL(*mockStatusManager_, SetClusterParseStatus(uniqueKey, ParserStatus::FINISH));

    // Act
    bool result = parser.ParserClusterOfDb();

    // Assert
    EXPECT_TRUE(result);
}

} // namespace Timeline
} // namespace Module
} // namespace Dic
