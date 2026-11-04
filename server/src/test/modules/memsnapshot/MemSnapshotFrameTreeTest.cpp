/* Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2: http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <fstream>
#include <future>
#include <thread>
#include <chrono>
#include "FileUtil.h"
#include "PythonUtil.h"
#include "MemSnapshotFrameTreeService.h"
#include "MemSnapshotSliceService.h"
#include "MemSnapshotProtocolRequest.h"
#include "MemSnapshotProtocolResponse.h"

using namespace Dic;
using namespace Dic::Module::MemSnapshot;

namespace {
std::string scriptResult;
std::string lastOutput;
std::string lastScript;
std::vector<std::string> lastArguments;
int scriptCalls = 0;
int scriptExitCode = 0;
bool omitOutput = false;
std::string mutateManifest;
std::function<void(const std::function<bool()> &)> scriptHook;

int ExecuteQueryScript(
    const std::string &script, const std::vector<std::string> &arguments, const std::optional<std::string> &) {
    ++scriptCalls;
    lastScript = script;
    lastArguments = arguments;
    lastOutput = arguments.back();
    const auto cancelPath = fs::u8path(arguments[9]);
    const auto cancelled = [cancelPath] { return fs::exists(cancelPath); };
    if (scriptHook) {
        scriptHook(cancelled);
    }
    if (cancelled()) {
        return -2;
    }
    if (!omitOutput) {
        std::ofstream file(fs::u8path(lastOutput), std::ios::binary);
        file << scriptResult;
    }
    if (!mutateManifest.empty()) {
        std::ofstream file(fs::u8path(mutateManifest), std::ios::app);
        file << " ";
    }
    return scriptExitCode;
}
}

class MemSnapshotFrameTreeAdapterTest : public testing::Test {
  protected:
    std::string source;
    std::string directory;
    static uint64_t sequence;

    void SetUp() override {
        source = FileUtil::SplicePath(testing::TempDir(), "snapshot_frame_adapter_" + std::to_string(++sequence));
        directory = source + ".msinsight";
        fs::create_directories(directory + "/device_0");
        for (const auto *name : {"slice_00000.db", "slice_00001.db"}) {
            std::ofstream file(directory + "/device_0/" + name);
            file << "source";
        }
        std::ofstream manifest(directory + "/manifest.json");
        manifest << R"({"schemaVersion":1,"sourceFile":"test.pkl","cacheHash":"test","eventsPerSlice":3,
            "status":"complete","devices":{"0":{"eventCount":6,"sliceCount":2,"readySlices":[0,1],"slices":[
            {"index":0,"startEventId":0,"endEventId":2,"file":"device_0/slice_00000.db","ready":true},
            {"index":1,"startEventId":3,"endEventId":5,"file":"device_0/slice_00001.db","ready":true}]}}})";
        scriptResult = R"({"version":1,"coverage":[1,1,1],"nodes":[
            {"nodeId":"root","parentId":null,"depth":0,"kind":"root","frame":null,
             "total":[9007199254740993,80,1],"self":[0,0,0],"childCount":1},
            {"nodeId":"frame/1","parentId":"root","depth":1,"kind":"frame",
             "frame":{"filename":"model.py","line":2,"name":"forward"},
             "total":[9007199254740993,80,1],"self":[9007199254740993,80,1],"childCount":0}]})";
        scriptCalls = 0;
        scriptExitCode = 0;
        omitOutput = false;
        mutateManifest.clear();
        lastOutput.clear();
        scriptHook = {};
        MemSnapshotFrameTreeService::ClearCache();
        MOCKER(PythonUtil::ExecuteScript).stubs().will(invoke(ExecuteQueryScript));
    }

    void TearDown() override {
        MemSnapshotFrameTreeService::ClearCache();
        GlobalMockObject::reset();
        fs::remove_all(fs::u8path(directory));
    }

    std::shared_ptr<const SnapshotFrameTree> Query(int64_t eventId = 3, bool includeStatic = true) {
        return MemSnapshotFrameTreeService::Query(source, "0", eventId, includeStatic);
    }
};
uint64_t MemSnapshotFrameTreeAdapterTest::sequence = 0;

TEST_F(MemSnapshotFrameTreeAdapterTest, CallsPythonOnceAndKeepsFullTreeForPagination) {
    const auto tree = Query();
    ASSERT_EQ(tree->nodes.size(), 2U);
    EXPECT_EQ(tree->nodes[0].total.sizeBytes, 9007199254740993ULL);
    EXPECT_EQ(tree->nodes[1].frame->name, "forward");
    EXPECT_EQ(tree->dynamicBlocks, 1U);
    EXPECT_EQ(tree->resolvedBlocks, 1U);
    EXPECT_EQ(tree->orderedFrameBlocks, 1U);
    EXPECT_EQ(lastScript, FileUtil::SplicePath("mem_snap_dump", "tools", "frame_tree.py"));
    EXPECT_EQ(lastArguments[0], "--manifest");
    EXPECT_EQ(lastArguments[1], MemSnapshotSliceService::GetManifestPath(source));
    EXPECT_EQ(lastArguments[3], "0");
    EXPECT_EQ(lastArguments[5], "3");
    EXPECT_EQ(lastArguments[7], "1");
    EXPECT_FALSE(fs::exists(fs::path(lastOutput).parent_path()));
    Protocol::MemSnapshotFrameTreeResponse response;
    response.tree = tree;
    response.pageSize = 1;
    for (uint64_t offset = 0; offset <= 2; ++offset) {
        response.offset = offset;
        EXPECT_TRUE(response.ToJson().has_value());
        EXPECT_EQ(Query(), tree);
    }
    EXPECT_EQ(scriptCalls, 1);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, InvalidatesOnQueryParametersDataAndReset) {
    const auto before = Query();
    EXPECT_NE(Query(4), before);
    EXPECT_NE(Query(4, false), before);
    EXPECT_EQ(lastArguments[7], "0");
    const auto cached = Query();
    {
        std::ofstream file(directory + "/device_0/slice_00000.db", std::ios::app);
        file << "changed";
    }
    EXPECT_NE(Query(), cached);
    MemSnapshotFrameTreeService::ClearCache();
    EXPECT_NE(Query(), cached);
    EXPECT_EQ(scriptCalls, 6);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, PassesUtf8PathsToPythonWithoutLocalEncodingConversion) {
    const auto original = fs::u8path(directory);
    source += u8"_中文路径";
    directory = source + ".msinsight";
    fs::rename(original, fs::u8path(directory));
    ASSERT_NO_THROW(Query());
    EXPECT_EQ(lastArguments[1], (fs::u8path(directory) / "manifest.json").u8string());
    EXPECT_EQ(fs::u8path(lastArguments[9]).u8string(), lastArguments[9]);
    EXPECT_EQ(fs::u8path(lastOutput).u8string(), lastOutput);
    EXPECT_FALSE(fs::exists(fs::u8path(lastOutput).parent_path()));
}

TEST_F(MemSnapshotFrameTreeAdapterTest, RejectsMissingPartialAndFailedProcessResultsWithoutCaching) {
    const auto valid = scriptResult;
    omitOutput = true;
    EXPECT_THROW(Query(), std::runtime_error);
    EXPECT_FALSE(fs::exists(fs::path(lastOutput).parent_path()));
    omitOutput = false;
    scriptResult = "{";
    EXPECT_THROW(Query(), std::runtime_error);
    scriptResult = valid;
    scriptExitCode = -1;
    EXPECT_THROW(Query(), std::runtime_error);
    scriptExitCode = 0;
    EXPECT_NO_THROW(Query());
    EXPECT_EQ(scriptCalls, 4);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, PropagatesPythonErrorsAndRejectsInvalidSchema) {
    for (const auto *result : {R"({"version":1,"error":"Frame tree metric overflow"})",
             R"({"version":2,"nodes":[],"coverage":[0,0,0]})", R"({"version":1,"nodes":[],"coverage":[0,0,0]})",
             R"({"version":1,"nodes":[{}],"coverage":[0,0,0]})", R"({"version":1,"nodes":{},"coverage":[0,0,0]})"}) {
        scriptResult = result;
        EXPECT_THROW(Query(), std::runtime_error);
        EXPECT_FALSE(fs::exists(fs::path(lastOutput).parent_path()));
    }
    EXPECT_EQ(scriptCalls, 5);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, RejectsDatasetMutationDuringQuery) {
    mutateManifest = directory + "/manifest.json";
    EXPECT_THROW(Query(), std::runtime_error);
    mutateManifest.clear();
    EXPECT_NO_THROW(Query());
    EXPECT_EQ(scriptCalls, 2);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, InvalidContextNeverStartsPython) {
    EXPECT_THROW(Query(6), std::runtime_error);
    EXPECT_THROW(MemSnapshotFrameTreeService::Query(source, "0;DROP", 3, true), std::runtime_error);
    fs::remove(directory + "/device_0/slice_00000.db");
    EXPECT_THROW(Query(), std::runtime_error);
    EXPECT_EQ(scriptCalls, 0);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, NewRequestCancelsRunningQueryAndCachesOnlyLatestResult) {
    std::promise<void> started;
    std::string cancelledOutput;
    scriptHook = [&](const std::function<bool()> &cancelled) {
        if (scriptCalls != 1) {
            return;
        }
        cancelledOutput = lastOutput;
        started.set_value();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!cancelled() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };
    MemSnapshotFrameTreeService::NotifyRequest(1);
    auto first = std::async(std::launch::async, [&]() {
        try {
            MemSnapshotFrameTreeService::Query(source, "0", 3, true, 1);
            return false;
        } catch (const std::runtime_error &) {
            return true;
        }
    });
    const auto status = started.get_future().wait_for(std::chrono::seconds(2));
    // 即使启动失败也先使任务失效，确保测试不会遗留阻塞线程。
    MemSnapshotFrameTreeService::NotifyRequest(2);
    EXPECT_EQ(status, std::future_status::ready);
    const auto latest = MemSnapshotFrameTreeService::Query(source, "0", 4, true, 2);
    EXPECT_TRUE(first.get());
    EXPECT_FALSE(fs::exists(fs::u8path(cancelledOutput).parent_path()));
    MemSnapshotFrameTreeService::NotifyRequest(3);
    EXPECT_EQ(MemSnapshotFrameTreeService::Query(source, "0", 4, true, 3), latest);
    EXPECT_EQ(scriptCalls, 2);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, SkipsSupersededQueuedRequestBeforeStartingPython) {
    MemSnapshotFrameTreeService::NotifyRequest(1);
    MemSnapshotFrameTreeService::NotifyRequest(2);
    EXPECT_THROW(MemSnapshotFrameTreeService::Query(source, "0", 3, true, 1), std::runtime_error);
    EXPECT_EQ(scriptCalls, 0);
    EXPECT_NO_THROW(MemSnapshotFrameTreeService::Query(source, "0", 4, true, 2));
    EXPECT_EQ(scriptCalls, 1);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, ResetCancelsRunningQueryWithoutWaitingOrPublishingResult) {
    scriptHook = [](const std::function<bool()> &cancelled) {
        MemSnapshotFrameTreeService::ClearCache();
        EXPECT_TRUE(cancelled());
    };
    EXPECT_THROW(Query(), std::runtime_error);
    EXPECT_FALSE(fs::exists(fs::u8path(lastOutput).parent_path()));
    scriptHook = {};
    EXPECT_NO_THROW(Query());
    EXPECT_EQ(scriptCalls, 2);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, ResetCancelsQueuedRequestBeforeStartingPython) {
    MemSnapshotFrameTreeService::NotifyRequest(1);
    MemSnapshotFrameTreeService::ClearCache();
    EXPECT_THROW(MemSnapshotFrameTreeService::Query(source, "0", 3, true, 1), std::runtime_error);
    EXPECT_EQ(scriptCalls, 0);
    MemSnapshotFrameTreeService::NotifyRequest(2);
    EXPECT_NO_THROW(MemSnapshotFrameTreeService::Query(source, "0", 4, true, 2));
    EXPECT_EQ(scriptCalls, 1);
}

TEST_F(MemSnapshotFrameTreeAdapterTest, NewRequestKeepsPreviouslyCompletedCacheUsable) {
    const auto cached = Query();
    scriptHook = [](const std::function<bool()> &) { MemSnapshotFrameTreeService::NotifyRequest(2); };
    MemSnapshotFrameTreeService::NotifyRequest(1);
    EXPECT_THROW(MemSnapshotFrameTreeService::Query(source, "0", 4, true, 1), std::runtime_error);
    scriptHook = {};
    EXPECT_EQ(MemSnapshotFrameTreeService::Query(source, "0", 3, true, 2), cached);
    EXPECT_EQ(scriptCalls, 2);
}

TEST(MemSnapshotFrameTreeResponseTest, OversizedPageFailsBudgetWithoutTruncatingIt) {
    auto tree = std::make_shared<SnapshotFrameTree>();
    for (size_t index = 0; index < 3; ++index) {
        SnapshotFrameNode node;
        node.nodeId = "frame/" + std::to_string(index);
        node.frame = SnapshotFrame{"model.py", 1, std::string(1024 * 1024, 'f')};
        tree->nodes.push_back(std::move(node));
    }
    Protocol::MemSnapshotFrameTreeResponse response;
    response.tree = tree;
    response.pageSize = 3;
    EXPECT_FALSE(response.CheckPageBudget());
    response.pageSize = 1;
    EXPECT_TRUE(response.CheckPageBudget());
    response.offset = 3;
    EXPECT_TRUE(response.CheckPageBudget());
}

TEST(MemSnapshotFrameTreeParamsTest, RejectsInvalidTypesRangesAndOverflow) {
    Protocol::MemSnapshotFrameTreeParams params;
    std::string error;
    params.deviceId = "0";
    params.currentPage = 1;
    params.pageSize = 1000;
    EXPECT_TRUE(params.CommonCheck(error));
    for (const auto value : {0LL, -1LL, 1001LL}) {
        params.pageSize = value;
        EXPECT_FALSE(params.CommonCheck(error));
    }
    params.pageSize = 1000;
    params.currentPage = INT64_MAX;
    EXPECT_FALSE(params.CommonCheck(error));
    params.currentPage = 0;
    EXPECT_FALSE(params.CommonCheck(error));
    params.currentPage = 1;
    params.eventId = 9007199254740992LL;
    EXPECT_FALSE(params.CommonCheck(error));
}

TEST(MemSnapshotFrameTreeParamsTest, ParsesStrictTypesAndDefaultsIncludeStatic) {
    const std::string prefix =
        R"({"id":1,"type":"request","moduleName":"leaks","command":"Memory/snapshot/frameTree","params":)";
    auto parse = [&](const std::string &params) {
        rapidjson::Document document;
        document.Parse((prefix + params + "}").c_str());
        std::string error;
        return Protocol::MemSnapshotFrameTreeRequest::FromJson(document, error);
    };
    auto request = parse(R"({"deviceId":"0","eventId":3,"currentPage":1,"pageSize":1000})");
    ASSERT_NE(request, nullptr);
    EXPECT_TRUE(static_cast<Protocol::MemSnapshotFrameTreeRequest &>(*request).params.includeStatic);
    for (const auto *params : {R"({"deviceId":0,"eventId":3,"currentPage":1,"pageSize":1})",
             R"({"deviceId":"0","eventId":3.5,"currentPage":1,"pageSize":1})",
             R"({"deviceId":"0","eventId":3,"currentPage":1,"pageSize":1,"includeStatic":1})",
             R"({"deviceId":"0","eventId":3,"currentPage":1})",
             R"({"deviceId":"0","eventId":3,"currentPage":0,"pageSize":1})",
             R"({"deviceId":"0","eventId":9007199254740992,"currentPage":1,"pageSize":1})"}) {
        EXPECT_EQ(parse(params), nullptr) << params;
    }
}
