/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#include <chrono>
#include <fstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "FileUtil.h"
#include "MemSnapshotSliceService.h"

using namespace Dic;
using namespace Dic::Module::MemSnapshot;

class MemSnapshotSliceServiceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        snapshotPath = FileUtil::SplicePath(
            ::testing::TempDir(), "mem_snapshot_slice_service_test_" + std::to_string(++caseSeq) + ".pkl");
        artifactPath = MemSnapshotSliceService::GetArtifactDirectory(snapshotPath);
        fs::remove_all(artifactPath);
        fs::create_directories(artifactPath);
    }

    void TearDown() override { fs::remove_all(artifactPath); }

    void WriteManifest(const std::string &content) const {
        std::ofstream manifest(MemSnapshotSliceService::GetManifestPath(snapshotPath), std::ios::trunc);
        ASSERT_TRUE(manifest.is_open());
        manifest << content;
    }

    static int caseSeq;
    std::string snapshotPath;
    std::string artifactPath;
};

int MemSnapshotSliceServiceTest::caseSeq = 0;

TEST_F(MemSnapshotSliceServiceTest, LoadsManifestAndDefaultsToLatestReadySlice) {
    WriteManifest(R"({
        "schemaVersion": 1,
        "status": "building",
        "sourceFile": "snapshot.pkl",
        "cacheHash": "hash",
        "eventsPerSlice": 100,
        "devices": {
            "0": {
                "eventCount": 300,
                "sliceCount": 3,
                "readySlices": [1, 2],
                "slices": [
                    {"index": 0, "startEventId": 0, "endEventId": 99, "file": "device_0/slice_00000.db", "ready": false},
                    {"index": 1, "startEventId": 100, "endEventId": 199, "file": "device_0/slice_00001.db", "ready": true},
                    {"index": 2, "startEventId": 200, "endEventId": 299, "file": "device_0/slice_00002.db", "ready": true}
                ]
            }
        }
    })");

    const auto manifest = MemSnapshotSliceService::LoadManifest(snapshotPath);
    ASSERT_TRUE(manifest.has_value());
    EXPECT_TRUE(manifest->IsValid());
    EXPECT_FALSE(manifest->IsComplete());
    const auto selected = MemSnapshotSliceService::ResolveSlice(*manifest, "0", -1);
    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(selected->index, 2);
    EXPECT_EQ(selected->startEventId, 200);
    EXPECT_EQ(selected->endEventId, 299);
}

TEST_F(MemSnapshotSliceServiceTest, RejectsUnknownUnreadyAndOutOfRangeSlices) {
    MemSnapshotSliceManifest manifest;
    manifest.schemaVersion = MEM_SNAPSHOT_SLICE_MANIFEST_SCHEMA_VERSION;
    manifest.status = "building";
    manifest.sourceFile = "snapshot.pkl";
    manifest.eventsPerSlice = 100;
    manifest.devices.emplace("0",
        MemSnapshotDeviceSliceInfo{
            200,
            2,
            {1},
            {{0, 0, 99, "device_0/slice_00000.db", false}, {1, 100, 199, "device_0/slice_00001.db", true}},
        });

    EXPECT_FALSE(MemSnapshotSliceService::ResolveSlice(manifest, "1", 1).has_value());
    EXPECT_FALSE(MemSnapshotSliceService::ResolveSlice(manifest, "0", 0).has_value());
    EXPECT_FALSE(MemSnapshotSliceService::ResolveSlice(manifest, "0", 2).has_value());
    EXPECT_TRUE(MemSnapshotSliceService::ResolveSlice(manifest, "0", 1).has_value());
}

TEST_F(MemSnapshotSliceServiceTest, ResolvesReadySliceByEventId) {
    MemSnapshotSliceManifest manifest;
    manifest.schemaVersion = MEM_SNAPSHOT_SLICE_MANIFEST_SCHEMA_VERSION;
    manifest.status = "complete";
    manifest.sourceFile = "snapshot.pkl";
    manifest.eventsPerSlice = 100;
    manifest.devices.emplace("0",
        MemSnapshotDeviceSliceInfo{
            200,
            2,
            {0, 1},
            {{0, 0, 99, "device_0/slice_00000.db", true}, {1, 100, 199, "device_0/slice_00001.db", true}},
        });

    const auto firstSlice = MemSnapshotSliceService::ResolveSliceByEventId(manifest, "0", 0);
    ASSERT_TRUE(firstSlice.has_value());
    EXPECT_EQ(firstSlice->index, 0);
    const auto secondSlice = MemSnapshotSliceService::ResolveSliceByEventId(manifest, "0", 150);
    ASSERT_TRUE(secondSlice.has_value());
    EXPECT_EQ(secondSlice->index, 1);
    EXPECT_FALSE(MemSnapshotSliceService::ResolveSliceByEventId(manifest, "0", -1).has_value());
    EXPECT_FALSE(MemSnapshotSliceService::ResolveSliceByEventId(manifest, "0", 200).has_value());
    EXPECT_FALSE(MemSnapshotSliceService::ResolveSliceByEventId(manifest, "1", 10).has_value());
}

TEST_F(MemSnapshotSliceServiceTest, RejectsSlicePathOutsideArtifactDirectory) {
    MemSnapshotSliceInfo slice{0, 0, 99, "../outside.db", true};
    EXPECT_TRUE(MemSnapshotSliceService::ResolveSliceDbPath(snapshotPath, slice).empty());

    slice.file = "device_0/slice_00000.db";
    const auto resolvedPath = MemSnapshotSliceService::ResolveSliceDbPath(snapshotPath, slice);
    EXPECT_FALSE(resolvedPath.empty());
    EXPECT_EQ(resolvedPath, FileUtil::SplicePath(artifactPath, slice.file));
}

TEST_F(MemSnapshotSliceServiceTest, RejectsMalformedManifest) {
    WriteManifest(R"({"schemaVersion":1,"status":"complete","devices":{}})");
    EXPECT_FALSE(MemSnapshotSliceService::LoadManifest(snapshotPath).has_value());
}

TEST_F(MemSnapshotSliceServiceTest, RejectsManifestWithNonContiguousSliceRanges) {
    WriteManifest(R"({
        "schemaVersion": 1,
        "status": "complete",
        "sourceFile": "snapshot.pkl",
        "cacheHash": "hash",
        "eventsPerSlice": 100,
        "devices": {
            "0": {
                "eventCount": 200,
                "sliceCount": 2,
                "readySlices": [0, 1],
                "slices": [
                    {"index": 0, "startEventId": 0, "endEventId": 99, "file": "device_0/slice_00000.db", "ready": true},
                    {"index": 1, "startEventId": 101, "endEventId": 200, "file": "device_0/slice_00001.db", "ready": true}
                ]
            }
        }
    })");

    EXPECT_FALSE(MemSnapshotSliceService::LoadManifest(snapshotPath).has_value());
}

TEST_F(MemSnapshotSliceServiceTest, RejectsManifestWithDuplicateSliceFiles) {
    WriteManifest(R"({
        "schemaVersion": 1,
        "status": "complete",
        "sourceFile": "snapshot.pkl",
        "cacheHash": "hash",
        "eventsPerSlice": 100,
        "devices": {
            "0": {
                "eventCount": 200,
                "sliceCount": 2,
                "readySlices": [0, 1],
                "slices": [
                    {"index": 0, "startEventId": 0, "endEventId": 99, "file": "device_0/slice.db", "ready": true},
                    {"index": 1, "startEventId": 100, "endEventId": 199, "file": "device_0/slice.db", "ready": true}
                ]
            }
        }
    })");

    EXPECT_FALSE(MemSnapshotSliceService::LoadManifest(snapshotPath).has_value());
}

TEST_F(MemSnapshotSliceServiceTest, ReloadsManifestWhenFileChanges) {
    WriteManifest(R"({
        "schemaVersion": 1,
        "status": "building",
        "sourceFile": "snapshot.pkl",
        "cacheHash": "hash",
        "eventsPerSlice": 100,
        "devices": {
            "0": {
                "eventCount": 100,
                "sliceCount": 1,
                "readySlices": [0],
                "slices": [
                    {"index": 0, "startEventId": 0, "endEventId": 99, "file": "device_0/slice_00000.db", "ready": true}
                ]
            }
        }
    })");
    const auto first = MemSnapshotSliceService::LoadManifest(snapshotPath);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->status, "building");

    WriteManifest(R"({
        "schemaVersion": 1,
        "status": "complete",
        "sourceFile": "snapshot.pkl",
        "cacheHash": "hash-reloaded",
        "eventsPerSlice": 100,
        "devices": {
            "0": {
                "eventCount": 100,
                "sliceCount": 1,
                "readySlices": [0],
                "slices": [
                    {"index": 0, "startEventId": 0, "endEventId": 99, "file": "device_0/slice_00000.db", "ready": true}
                ]
            }
        }
    })");
    const auto manifestPath = MemSnapshotSliceService::GetManifestPath(snapshotPath);
    std::error_code error;
    const auto currentWriteTime = fs::last_write_time(manifestPath, error);
    ASSERT_FALSE(error);
    fs::last_write_time(manifestPath, currentWriteTime + std::chrono::seconds(1), error);
    ASSERT_FALSE(error);

    const auto second = MemSnapshotSliceService::LoadManifest(snapshotPath);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->status, "complete");
}
