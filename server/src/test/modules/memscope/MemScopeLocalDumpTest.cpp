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

#include <filesystem>
#include <gtest/gtest.h>
#include "DataBaseManager.h"
#include "MemScopeAllocationDataProcessor.h"
#include "MemScopeParser.h"
#include "MemScopeProtocolRequest.h"
#include "MemScopeProtocolResponse.h"
#include "TestSuit.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::MemScope;
using namespace Dic::Module::FullDb;
using namespace Dic::Protocol;

namespace {
// test_data/memscope/attr_usage/: 1/10 prefix of a memscope dump whose Attr has total/process_used/device_used.
std::string FixtureDumpPath(const std::string &fileName) {
    return TestSuit::GetTestDataFile("memscope", "attr_usage", fileName);
}

std::string CopyDumpForParse(const std::string &srcPath, const std::string &name) {
    const auto workDir = std::filesystem::temp_directory_path() / "memscope_attr_usage_test";
    std::filesystem::create_directories(workDir);
    const auto dest = workDir / name;
    std::filesystem::copy_file(srcPath, dest, std::filesystem::copy_options::overwrite_existing);
    return dest.string();
}

void ParseDump(const std::string &dbPath) {
    DataBaseManager::Instance().Clear();
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    ASSERT_TRUE(memoryDatabase->OpenDb(dbPath, false));
    ASSERT_TRUE(memoryDatabase->DropMemoryAllocationAndBlockTable());
    ASSERT_TRUE(MemScopeParser::ParseMemoryMemScopeDumpEventsAndPythonTraces("0"));
}

void QueryPtaAllocations(std::vector<MemoryAllocation> &allocations) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    MemScopeMemoryAllocationParams params;
    params.deviceId = "0";
    params.eventType = "PTA";
    params.optimized = false;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
}

bool HasPositive(const std::vector<MemoryAllocation> &allocations, uint64_t MemoryAllocation::*field) {
    for (const auto &allocation : allocations) {
        if (allocation.*field > 0) {
            return true;
        }
    }
    return false;
}
} // namespace

class MemScopeLocalDumpTest : public ::testing::Test {
  protected:
    static void TearDownTestSuite() {
        auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
        if (memoryDatabase != nullptr) {
            memoryDatabase->CloseDb();
        }
        DataBaseManager::Instance().Clear();
    }
};

TEST_F(MemScopeLocalDumpTest, NewDumpPopulatesReservedProcessAndDeviceLines) {
    const auto src = FixtureDumpPath("memscope_dump_20260820113144.db");
    ASSERT_TRUE(std::filesystem::exists(src)) << src;
    const auto dbPath = CopyDumpForParse(src, "memscope_local_new.db");
    ParseDump(dbPath);

    std::vector<MemoryAllocation> allocations;
    QueryPtaAllocations(allocations);
    ASSERT_FALSE(allocations.empty());
    EXPECT_TRUE(HasPositive(allocations, &MemoryAllocation::reservedSize));
    EXPECT_TRUE(HasPositive(allocations, &MemoryAllocation::processUsed));
    EXPECT_TRUE(HasPositive(allocations, &MemoryAllocation::deviceUsed));

    const auto reservedLine = MemScopeAllocationDataProcessor::CompressReservedLine(allocations);
    const auto processUsedLine = MemScopeAllocationDataProcessor::CompressProcessUsedLine(allocations);
    const auto deviceUsedLine = MemScopeAllocationDataProcessor::CompressDeviceUsedLine(allocations);
    EXPECT_FALSE(reservedLine.empty());
    EXPECT_FALSE(processUsedLine.empty());
    EXPECT_FALSE(deviceUsedLine.empty());

    MemScopeMemoryAllocationsResponse response;
    response.reservedLine = reservedLine;
    response.processUsedLine = processUsedLine;
    response.deviceUsedLine = deviceUsedLine;
    const auto json = response.ToJson();
    ASSERT_TRUE(json.has_value());
    EXPECT_GT((*json)["body"]["reservedLine"].Size(), 0);
    EXPECT_GT((*json)["body"]["processUsedLine"].Size(), 0);
    EXPECT_GT((*json)["body"]["deviceUsedLine"].Size(), 0);
}
