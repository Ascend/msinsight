#include <filesystem>
#include <sqlite3.h>

#include <gtest/gtest.h>

#include "DataBaseManager.h"
#include "OverlapAnsRepo.h"
#include "ProjectParserFactory.h"
#include "RankLaneMergeCoordinator.h"
#include "RankOverlapStore.h"
#include "TestSuit.h"

using namespace Dic::Module::Timeline;

namespace {
int64_t QueryInt64(const std::string &path, const char *sql) {
    sqlite3 *database = nullptr;
    if (sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI | SQLITE_OPEN_FULLMUTEX,
            nullptr) != SQLITE_OK) {
        sqlite3_close(database);
        return -1;
    }
    sqlite3_stmt *statement = nullptr;
    int64_t result = -1;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        result = sqlite3_column_int64(statement, 0);
    }
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return result;
}

bool CreateSourceOverlapDatabase(const std::string &path) {
    sqlite3 *database = nullptr;
    if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
        sqlite3_close(database);
        return false;
    }
    const bool success = sqlite3_exec(database,
                             "CREATE TABLE OVERLAP_ANALYSIS(id INTEGER PRIMARY KEY AUTOINCREMENT,deviceId "
                             "INTEGER,startNs INTEGER,endNs INTEGER,type INTEGER);",
                             nullptr, nullptr, nullptr) == SQLITE_OK;
    sqlite3_close(database);
    return success;
}

class TestProjectParserBase : public Dic::Module::ProjectParserBase {
  public:
    static void Search(const std::string &rankId, const std::string &fileId,
        std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> &metadata) {
        SearchMetaData(rankId, fileId, metadata);
    }
};
}

class RankOverlapStoreTest : public testing::Test {
  protected:
    void TearDown() override {
        RankOverlapStore::Instance().SetPublishFailurePointForTesting(
            RankOverlapStore::PublishFailurePointForTesting::NONE);
        RankOverlapStore::Instance().Clear();
        RankLaneMergeCoordinator::Instance().Reset();
    }
};

TEST_F(RankOverlapStoreTest, FailedRefreshKeepsPreviouslyPublishedSource) {
    const std::string rankId = "rank-overlap-refresh-test";
    auto &store = RankOverlapStore::Instance();
    const std::string originalSource = store.Publish(rankId, "7", {{10, 20, 0}});
    ASSERT_FALSE(originalSource.empty());
    ASSERT_EQ(QueryInt64(originalSource, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS"), 1);

    store.SetPublishFailurePointForTesting(RankOverlapStore::PublishFailurePointForTesting::WRITE_DATABASE);
    EXPECT_TRUE(store.Publish(rankId, "7", {{20, 30, 1}}).empty());
    EXPECT_EQ(store.ResolveSource(rankId), originalSource);
    EXPECT_NE(DataBaseManager::Instance().GetTraceDatabaseByFileId(originalSource), nullptr);
    EXPECT_EQ(QueryInt64(originalSource, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS"), 1);

    store.SetPublishFailurePointForTesting(RankOverlapStore::PublishFailurePointForTesting::CREATE_CONNECTION_POOL);
    EXPECT_TRUE(store.Publish(rankId, "7", {{30, 40, 2}}).empty());
    EXPECT_EQ(store.ResolveSource(rankId), originalSource);
    EXPECT_NE(DataBaseManager::Instance().GetTraceDatabaseByFileId(originalSource), nullptr);
    EXPECT_EQ(QueryInt64(originalSource, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS"), 1);
}

TEST_F(RankOverlapStoreTest, PublishRegistersQueryableDerivedSourceAndInvalidateRemovesIt) {
    const std::vector<Dic::Module::FullDb::OVERLAP_INFO> rows = {
        {10, 20, 0},
        {20, 30, 2},
    };

    const std::string source = RankOverlapStore::Instance().Publish("rank-overlap-store-test", "7", rows);

    ASSERT_FALSE(source.empty());
    EXPECT_EQ(RankOverlapStore::Instance().ResolveSource("rank-overlap-store-test"), source);
    EXPECT_EQ(source.find("file:msinsight_overlap_"), 0);
    EXPECT_NE(source.find("mode=memory"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(source));
    EXPECT_NE(DataBaseManager::Instance().GetTraceDatabaseByFileId(source), nullptr);
    EXPECT_EQ(DataBaseManager::Instance().FindDeviceIdByFileIdAndRankId(source, "rank-overlap-store-test"), "7");
    EXPECT_EQ(QueryInt64(source, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS"), 2);
    EXPECT_EQ(QueryInt64(source, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS WHERE deviceId = 7"), 2);

    RankOverlapStore::Instance().Invalidate("rank-overlap-store-test");

    EXPECT_TRUE(RankOverlapStore::Instance().ResolveSource("rank-overlap-store-test").empty());
    EXPECT_EQ(DataBaseManager::Instance().GetTraceDatabaseByFileId(source), nullptr);
    EXPECT_TRUE(DataBaseManager::Instance().FindDeviceIdByFileIdAndRankId(source, "rank-overlap-store-test").empty());
    EXPECT_EQ(QueryInt64(source, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS"), -1);
}

TEST_F(RankOverlapStoreTest, DifferentRanksUseIndependentSharedMemoryDatabases) {
    const std::string firstSource = RankOverlapStore::Instance().Publish("rank-overlap-first", "0", {{10, 20, 0}});
    const std::string secondSource =
        RankOverlapStore::Instance().Publish("rank-overlap-second", "1", {{20, 30, 1}, {30, 40, 2}});

    ASSERT_FALSE(firstSource.empty());
    ASSERT_FALSE(secondSource.empty());
    EXPECT_NE(firstSource, secondSource);
    EXPECT_EQ(QueryInt64(firstSource, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS"), 1);
    EXPECT_EQ(QueryInt64(secondSource, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS"), 2);

    RankOverlapStore::Instance().Invalidate("rank-overlap-first");

    EXPECT_EQ(QueryInt64(firstSource, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS"), -1);
    EXPECT_EQ(QueryInt64(secondSource, "SELECT COUNT(*) FROM OVERLAP_ANALYSIS"), 2);
}

TEST_F(RankOverlapStoreTest, MergedMetadataInitializesDerivedOverlapNames) {
    const std::string rankId = "rank-overlap-metadata-test";
    const std::string firstSource = TestSuit::GetTestDataFile("rank_overlap_source_1.db");
    const std::string secondSource = TestSuit::GetTestDataFile("rank_overlap_source_2.db");
    ASSERT_TRUE(CreateSourceOverlapDatabase(firstSource));
    ASSERT_TRUE(CreateSourceOverlapDatabase(secondSource));
    auto &manager = DataBaseManager::Instance();
    manager.SetDataType(DataType::DB, firstSource);
    manager.SetDataType(DataType::DB, secondSource);
    ASSERT_TRUE(manager.CreateTraceConnectionPool(rankId, firstSource));
    ASSERT_TRUE(manager.CreateTraceConnectionPool(rankId, secondSource));
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource(rankId, firstSource);
    coordinator.RegisterSource(rankId, secondSource);
    coordinator.MarkSourceSucceeded(rankId, firstSource);
    coordinator.MarkSourceSucceeded(rankId, secondSource);
    const std::string derivedSource = RankOverlapStore::Instance().Publish(rankId, "0", {{10, 20, 0}});
    ASSERT_FALSE(derivedSource.empty());

    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metadata;
    TestProjectParserBase::Search(rankId, firstSource, metadata);
    OverlapAnsRepo repository;
    SliceQuery query;
    query.rankId = rankId;
    query.dbPath = derivedSource;
    std::vector<CompeteSliceDomain> slices;
    repository.QueryCompeteSliceByIds(query, {1}, slices);

    ASSERT_EQ(slices.size(), 1);
    EXPECT_EQ(slices.front().name, "Computing");

    Dic::Protocol::UnitThreadsParams params;
    params.rankId = rankId;
    params.dbPath = derivedSource;
    params.startTime = 0;
    params.endTime = 30;
    params.metadataList.emplace_back(Dic::Protocol::Metadata{
        .tid = "0",
        .pid = "OVERLAP_ANALYSIS",
        .metaType = "OVERLAP_ANALYSIS",
    });
    Dic::Protocol::UnitThreadsBody body;
    auto derivedDatabase = manager.GetTraceDatabaseByFileId(derivedSource);
    ASSERT_NE(derivedDatabase, nullptr);
    ASSERT_TRUE(derivedDatabase->QueryThreads(params, body, 0, {0}));
    ASSERT_EQ(body.data.size(), 1);
    EXPECT_EQ(body.data.front().title, "Computing");

    manager.ReleaseDatabaseByFileId(firstSource);
    manager.ReleaseDatabaseByFileId(secondSource);
    std::filesystem::remove(firstSource);
    std::filesystem::remove(secondSource);
}
