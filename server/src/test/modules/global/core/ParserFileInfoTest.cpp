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
#include <vector>
#include "SystemMemoryDatabaseDef.h"

using namespace Dic::Module::Global;

class ParserFileInfoTest : public testing::Test {
  public:
    static std::shared_ptr<ParseFileInfo> BuildHierarchicalFileInfo(std::vector<std::string> &&hierarchical) {
        if (hierarchical.size() != 3) { // expect hierarchical size is 3
            return nullptr;
        }
        auto project = std::make_shared<ParseFileInfo>();
        project->subId = hierarchical[0]; // project index is 0
        project->type = ParseFileType::PROJECT;
        auto cluster = std::make_shared<ParseFileInfo>();
        cluster->subId = hierarchical[1]; // cluster index is 1
        cluster->type = ParseFileType::CLUSTER;
        auto rank = std::make_shared<ParseFileInfo>();
        rank->subId = hierarchical[2]; // rank index is 2
        rank->type = ParseFileType::RANK;
        cluster->subParseFile.push_back(rank);
        project->subParseFile.push_back(cluster);
        return project;
    }
};

TEST_F(ParserFileInfoTest, SerializeToJson) {
    auto info = BuildHierarchicalFileInfo({"project", "cluster", "rank1"});
    std::string expect = "[{\"clusterPath\":\"\",\"rankId\":\"\",\"host\":\"\",\"filePath\":\"\",\"type\":\"PROJECT\","
                         "\"deviceId\":\"\",\"fileDir\":\"\",\"projectType\":0,"
                         "\"children\":[{\"clusterPath\":\"\",\"rankId\":\"\",\"host\":\"\",\"filePath\":\"\","
                         "\"type\":\"CLUSTER\",\"deviceId\":\"\",\"fileDir\":\"\",\"projectType\":0,"
                         "\"children\":[{\"clusterPath\":\"\",\"rankId\":\"\","
                         "\"host\":\"\",\"filePath\":\"\",\"type\":\"RANK\","
                         "\"deviceId\":\"\",\"fileDir\":\"\",\"projectType\":0,\"children\":[]}]}]}]";
    document_t json(rapidjson::kArrayType);
    auto &allocator = json.GetAllocator();
    json.PushBack(info->SerializeToJson(allocator), allocator);
    std::string result = JsonUtil::JsonDump(json);
    EXPECT_EQ(result, expect);
}

TEST_F(ParserFileInfoTest, GetChildren) {
    auto info = BuildHierarchicalFileInfo({"project", "cluster", "rank1"});
    auto children = info->GetChildren();
    EXPECT_EQ(children.size(), 2); // expect children size is 2
    EXPECT_EQ(children[0]->subId, "cluster"); // cluster index is 0
    EXPECT_EQ(children[1]->subId, "rank1"); // rank1 index is 1
}
