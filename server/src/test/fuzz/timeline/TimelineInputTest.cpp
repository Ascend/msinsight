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
#include "TraceFileParser.h"

TEST(TraceFileParser, Parse) {
    char testApi[] = "test_trace_file_parser";
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        std::string outputFilePath = "./" + std::string(testApi) + ".json";
        ;
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

            // 将变异后的文件内容传给TraceFileParser进行解析
            std::vector<std::string> pathList = {outputFilePath};
            std::string rankId = "0";
            std::string folder = "folder";
            Dic::Module::Timeline::TraceFileParser::Instance().Parse(pathList, rankId, folder, rankId);

        } else {
            std::cout << "Generate mutation file failed." << std::endl;
        }
    }
    DT_FUZZ_END()
}
