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

#include <fstream>
#include "FuzzDefs.h"
#include "FuzzFileUtil.h"
#include "FileUtil.h"
#include "JsonUtil.h"
#include "FileSelector.h"

using namespace Dic;
using namespace Dic::Protocol;
using namespace Dic::Module::Global;
TEST(TestFileUtil, IsAbsolutePath) {
    char testApi[] = "test_file_util_is_absolute_path";
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        char *filePath = DT_SetGetString(&g_Element[0], 5, UINT32_MAX, "path");
        FileUtil::IsAbsolutePath(filePath);
    }
    DT_FUZZ_END()
}

TEST(TestFileUtil, CopyFileByPath) {
    char testApi[] = "test_file_util_copy_file_by_path";
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        std::string binFilePath = "./test_file_util_copy_file_by_path.bin";
        std::ofstream binFile(binFilePath, std::ios::binary | std::ios::trunc);

        char *fileContent = DT_SetGetBlob(&g_Element[0], 2, UINT32_MAX, "a");
        int fileSize = DT_GET_MutatedValueLen(&g_Element[0]);
        binFile.write(fileContent, fileSize);
        binFile.close();

        std::string copyPath = "./test_file_util_copy_file_by_path.copy";
        FileUtil::CopyFileByPath(binFilePath, copyPath);
    }
    DT_FUZZ_END()
}

TEST(TestFileUtil, PathPreprocess) {
    char testApi[] = "test_file_util_path_preprocess";
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        char *filePath = DT_SetGetString(&g_Element[0], 4, PATH_MAX, "./a");
        FileUtil::PathPreprocess(filePath);
    }
    DT_FUZZ_END()
}

TEST(JsonUtil, TryParse) {
    char testApi[] = "test_json_util_try_parse";
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        std::string outputFilePath = "./" + std::string(testApi) + ".json";
        std::string inputFilePath = "./test_data/trace_view_tiny.json";

        char *fileContent = nullptr;
        int fileContentSize = 0;
        if (GenerateFileMutation(inputFilePath, &fileContent, fileContentSize) == 0) {
            std::ofstream outputFile(outputFilePath, std::ios::trunc);
            if (!outputFile) {
                std::cout << "open output file failed: " << outputFilePath << std::endl;
                return;
            }
            outputFile.write(fileContent, fileContentSize);
            outputFile.close();
            std::string errMsg;
            JsonUtil::TryParse<kParseDefaultFlags>(fileContent, errMsg);
        } else {
            std::cout << "Generate mutation file failed." << std::endl;
        }
    }
    DT_FUZZ_END()
}

// 该用例用于测试在GetFilesHandler中的核心方法，获取某个目录下的子目录和文件等(在通过一系列安全校验的过滤下)
TEST(TestFileUtil, GetFoldersAndFiles) {
    char testApi[] = "test_get_folders_and_files";
    PathFuzzer pathFuzzer;
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        if (!pathFuzzer.ClearBaseDir()) {
            std::cout << "Init base dir failed." << std::endl;
            return;
        }

        std::vector<std::string> fileList;
        std::vector<std::string> dirList;
        uint pathCount = 10;
        pathFuzzer.GenerateFilePathMutation(pathCount, fileList, dirList);
        std::vector<std::unique_ptr<Protocol::Folder>> childrenFolders;
        std::vector<std::unique_ptr<Protocol::File>> childrenFiles;
        bool exists = false;
        FileSelector::GetFoldersAndFiles(pathFuzzer.baseDir, childrenFolders, childrenFiles, exists);
        EXPECT_TRUE(exists);
        EXPECT_EQ(childrenFolders.size(), dirList.size());
        EXPECT_EQ(childrenFiles.size(), fileList.size());
        if (!pathFuzzer.ClearBaseDir()) {
            std::cout << "Clear base dir failed." << std::endl;
            return;
        }
    }
    DT_FUZZ_END()
}
