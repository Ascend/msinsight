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
#include <fstream>
#include "ImportActionHandler.h"
#include "HandlerTest.cpp"

class ImportActionHandlerTest : HandlerTest {};

TEST_F(HandlerTest, ImportActionHandlerTestPathIsEmpty) {
    Dic::Module::Timeline::ImportActionHandler importActionHandler;
    std::unique_ptr<Dic::Protocol::Request> requestPtr = std::make_unique<Dic::Protocol::ImportActionRequest>();
    importActionHandler.HandleRequest(std::move(requestPtr));
}

TEST_F(HandlerTest, ImportActionHandlerTestPathIsNotEmpty) {
    Dic::Module::Timeline::ImportActionHandler importActionHandler;
    std::unique_ptr<Dic::Protocol::ImportActionRequest> requestPtr =
        std::make_unique<Dic::Protocol::ImportActionRequest>();
    requestPtr.get()->params.path.emplace_back("jjjj");
    importActionHandler.HandleRequest(std::move(requestPtr));
}

// 该用例测试导入cluster_analysis_output的上一级目录，这样导入是允许的
TEST_F(HandlerTest, ImportActionHandlerTestParentDirectoryOfClusterAnalysisOutput) {
#ifdef _WIN32
    const std::string deleteFolderPath = Dic::FileUtil::GetCurrPath() + "..\\..\\..\\..\\test\\data\\test\\";
    const std::string folderPath = deleteFolderPath + "cluster_analysis_output\\";
#else
    const std::string deleteFolderPath = Dic::FileUtil::GetCurrPath() + "../../../../test/data/test/";
    const std::string folderPath = deleteFolderPath + "cluster_analysis_output/";
#endif
    const std::string filePath1 = folderPath + "cluster_communication.json";
    const std::string filePath2 = folderPath + "cluster_communication_matrix.json";
    const std::string filePath3 = folderPath + "cluster_step_trace_time.csv";
    const std::string filePath4 = folderPath + "communication_group.json";
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    std::ofstream outfile;
    outfile.open(filePath1, std::ios::out | std::ios::trunc);
    outfile.close();
    outfile.open(filePath2, std::ios::out | std::ios::trunc);
    outfile.close();
    outfile.open(filePath3, std::ios::out | std::ios::trunc);
    outfile.close();
    outfile.open(filePath4, std::ios::out | std::ios::trunc);
    outfile.close();

    Dic::Module::Timeline::ImportActionHandler importActionHandler;
    std::unique_ptr<Dic::Protocol::ImportActionRequest> requestPtr =
        std::make_unique<Dic::Protocol::ImportActionRequest>();
    requestPtr.get()->params.projectName = "test";
    requestPtr.get()->params.projectAction = Dic::Protocol::ProjectActionEnum::ADD_FILE;
    requestPtr.get()->params.path.emplace_back(deleteFolderPath);
    bool result = importActionHandler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);

    Dic::Module::ParserFactory::Reset();
#ifdef _WIN32
    const std::string rmCommand = "rd /s /q " + deleteFolderPath;
#else
    const std::string rmCommand = "rm -r " + deleteFolderPath;
#endif
    system(rmCommand.c_str());
}

TEST_F(HandlerTest, ImportActionHandlerTestParentDirectoryOfClusterAnalysisOutputTransferProject) {
#ifdef _WIN32
    const std::string deleteFolderPath = Dic::FileUtil::GetCurrPath() + "..\\..\\..\\..\\test\\data\\test\\";
    const std::string folderPath = deleteFolderPath + "cluster_analysis_output\\";
#else
    const std::string deleteFolderPath = Dic::FileUtil::GetCurrPath() + "../../../../test/data/test/";
    const std::string folderPath = deleteFolderPath + "cluster_analysis_output/";
#endif
    const std::string filePath1 = folderPath + "cluster_communication.json";
    const std::string filePath2 = folderPath + "cluster_communication_matrix.json";
    const std::string filePath3 = folderPath + "cluster_step_trace_time.csv";
    const std::string filePath4 = folderPath + "communication_group.json";
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    std::ofstream outfile;
    outfile.open(filePath1, std::ios::out | std::ios::trunc);
    outfile.close();
    outfile.open(filePath2, std::ios::out | std::ios::trunc);
    outfile.close();
    outfile.open(filePath3, std::ios::out | std::ios::trunc);
    outfile.close();
    outfile.open(filePath4, std::ios::out | std::ios::trunc);
    outfile.close();

    Dic::Module::Timeline::ImportActionHandler importActionHandler;
    std::unique_ptr<Dic::Protocol::ImportActionRequest> requestPtr =
        std::make_unique<Dic::Protocol::ImportActionRequest>();
    requestPtr.get()->params.projectName = "test";
    requestPtr.get()->params.projectAction = Dic::Protocol::ProjectActionEnum::TRANSFER_PROJECT;
    requestPtr.get()->params.path.emplace_back(deleteFolderPath);
    bool result = importActionHandler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);

    Dic::Module::ParserFactory::Reset();
#ifdef _WIN32
    const std::string rmCommand = "rd /s /q " + deleteFolderPath;
#else
    const std::string rmCommand = "rm -r " + deleteFolderPath;
#endif
    system(rmCommand.c_str());
}
