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
#include "BaselineManagerService.h"
#include "ParserStatusManager.h"
#include "FileSelector.h"
#include "GlobalProtocolResponse.h"
#include "ProtocolDefs.h"
#include "FileUtil.h"
#include "TestSuit.h"

using namespace Dic::Module::Global;
using namespace Dic::Protocol;
class FileSelectorTest : public ::testing::Test {
  protected:
    inline static std::string testDataDir = TestSuit::GetTestDataFile();
};

TEST_F(FileSelectorTest, TestPathNotExist) {
    std::string path = "xxx";
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist);
    EXPECT_FALSE(exist);
    ASSERT_TRUE(childrenFolders.empty());
    ASSERT_TRUE(childrenFiles.empty());
}

TEST_F(FileSelectorTest, TestContainFolder) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "msprof");
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<Folder>> realChildrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    bool exist = true;
    std::vector<std::string> folders = {"normal", "slice"};
    for (const auto &folder : folders) {
        auto folderPtr = std::make_unique<Folder>();
        folderPtr->name = folder;
        folderPtr->path = Dic::FileUtil::SplicePath(path, folder);
        realChildrenFolders.emplace_back(std::move(folderPtr));
    }
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist);
    EXPECT_TRUE(exist);
    for (size_t i = 0; i < childrenFolders.size(); i++) {
        ASSERT_EQ(childrenFolders[i]->name, realChildrenFolders[i]->name);
    }
    ASSERT_TRUE(childrenFiles.empty());
}

TEST_F(FileSelectorTest, TestContainFiles) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "full_db");
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    std::vector<std::unique_ptr<File>> realChildrenFiles;
    bool exist = true;
    std::vector<std::string> files = {
        "ascend_pytorch_profiler.db", "cluster_analysis.db", "leaks_dump_20250806.dat", "msprof_0.db"};
    for (const auto &file : files) {
        std::string tmpPath = Dic::FileUtil::SplicePath(path, file);
        realChildrenFiles.emplace_back(std::make_unique<File>(file, tmpPath));
    }
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist);
    EXPECT_TRUE(exist);
    for (size_t i = 0; i < realChildrenFiles.size(); i++) {
        ASSERT_EQ(realChildrenFiles[i]->name, childrenFiles[i]->name);
    }
    ASSERT_TRUE(childrenFolders.empty());
}

TEST_F(FileSelectorTest, TestContainFolderandFiles) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "test_rank_0");
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<Folder>> realChildrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    std::vector<std::unique_ptr<File>> realChildrenFiles;
    bool exist = true;
    std::vector<std::string> folders = {"ASCEND_PROFILER_OUTPUT"};
    for (const auto &folder : folders) {
        auto folderPtr = std::make_unique<Folder>();
        folderPtr->name = folder;
        folderPtr->path = Dic::FileUtil::SplicePath(path, folder);
        realChildrenFolders.emplace_back(std::move(folderPtr));
    }
    std::vector<std::string> files = {"profiler_info_0.json"};
    for (const auto &file : files) {
        std::string tmpPath = Dic::FileUtil::SplicePath(path, file);
        realChildrenFiles.emplace_back(std::make_unique<File>(file, tmpPath));
    }
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist);
    EXPECT_TRUE(exist);
    for (size_t i = 0; i < childrenFolders.size(); i++) {
        ASSERT_EQ(childrenFolders[i]->name, realChildrenFolders[i]->name);
    }
    for (size_t i = 0; i < realChildrenFiles.size(); i++) {
        ASSERT_EQ(realChildrenFiles[i]->name, childrenFiles[i]->name);
    }
}
