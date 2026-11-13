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
#include "ExportAffinityAPIAdvice.h"
#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"
#include "AdvisorErrorManager.h"
#include "TestSuit.h"
#include "ProjectExplorerManager.h"
#include "AffinityAPIAdvisor.h"
#include "utils/Utf8TestUtil.h"
#include "ExportOpDetailsHandler.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <set>
#include <limits>

namespace Dic::Module::Advisor {
using namespace Dic::Protocol;

namespace {
// 统计一行中的CSV字段数，引号内的逗号不拆分，用于校验导出列不会错位。
size_t CountCsvFields(const std::string &line) {
    size_t fields = 1;
    bool inQuotes = false;
    for (const char character : line) {
        if (character == '"') {
            // 转义引号成对出现，连续切换两次后仍保持原来的字段状态。
            inQuotes = !inQuotes;
        } else if (character == ',' && !inQuotes) {
            ++fields;
        }
    }
    return fields;
}
} // namespace

class ExportAffinityAPIAdviceTest : public ::testing::Test {
  public:
    static int Main(int argc, char **argv) {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
};

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenRankIdEmpty) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenOrderByIllegal) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "test";
    request.orderBy = "duration--";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenStartTimeGreaterThanEndTime) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "test";
    request.startTime = 100;
    request.endTime = 50;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(ExportAffinityAPIAdviceTest, EscapeCsvFieldQuotesOnlyWhenSeparatorOrQuoteOrNewlinePresent) {
    EXPECT_EQ(ExportAffinityAPIAdvice::EscapeCsvField("aten::add"), "aten::add");
    EXPECT_EQ(ExportAffinityAPIAdvice::EscapeCsvField(""), "");
    EXPECT_EQ(ExportAffinityAPIAdvice::EscapeCsvField("\\=formula"), "\\=formula");
    EXPECT_EQ(ExportAffinityAPIAdvice::EscapeCsvField("aten::mul, aten::sigmoid"), "\"aten::mul, aten::sigmoid\"");
    EXPECT_EQ(ExportAffinityAPIAdvice::EscapeCsvField("say \"hi\""), "\"say \"\"hi\"\"\"");
    EXPECT_EQ(ExportAffinityAPIAdvice::EscapeCsvField("line1\nline2"), "\"line1\nline2\"");
    EXPECT_EQ(ExportAffinityAPIAdvice::EscapeCsvField("line1\r\nline2"), "\"line1\r\nline2\"");
    EXPECT_EQ(CountCsvFields(ExportAffinityAPIAdvice::EscapeCsvField("aten::mul, aten::sigmoid")), 1U);
}

TEST_F(ExportAffinityAPIAdviceTest, SanitizeFileNameKeepsCardIdAndReplacesPathCharacters) {
    EXPECT_EQ(
        ExportAffinityAPIAdvice::SanitizeFileName("node18899436934890168541_0 3"), "node18899436934890168541_0 3");
    EXPECT_EQ(ExportAffinityAPIAdvice::SanitizeFileName(
                  "D:\\test\\async_task_queue\\ASCEND_PROFILER_OUTPUT\\trace_view.json"),
        "D__test_async_task_queue_ASCEND_PROFILER_OUTPUT_trace_view.json");
    EXPECT_EQ(ExportAffinityAPIAdvice::SanitizeFileName("rank<>:\"|?*id"), "rank_______id");
    EXPECT_EQ(ExportAffinityAPIAdvice::SanitizeFileName("rank\nid"), "rank_id");
    const auto sanitized = ExportAffinityAPIAdvice::SanitizeFileName("D:\\demo &%$;'`\x7F\\trace_view.json");
    EXPECT_EQ(sanitized, "D__demo ________trace_view.json");
    EXPECT_FALSE(FileUtil::CheckPathInvalidChar(sanitized));
    EXPECT_EQ(ExportAffinityAPIAdvice::SanitizeFileName("前端工程"), "前端工程");
}

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenNoDatabase) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "nonexistent_rank";
    request.orderBy = "duration";
    request.orderType = "descend";
    request.startTime = 0;
    request.endTime = 0;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

class ExportAffinityAPIIntegrationTest : public TestSuit {
  protected:
    std::filesystem::path exportDir;
    const std::string projectName = "advisor_export_ut";

    void SetUp() override {
        exportDir = std::filesystem::path(TestSuit::GetSrcTestPath()) / "advisor_export_ut";
        std::filesystem::create_directories(exportDir);
        Global::ProjectExplorerManager::Instance().ClearProjectExplorer({projectName});
    }

    void TearDown() override {
        Global::ProjectExplorerManager::Instance().DeleteProjectAndFilePath(projectName, {});
        for (const auto &file : std::filesystem::directory_iterator(exportDir)) {
            if (file.path().extension() == ".csv") {
                std::filesystem::remove(file.path());
            }
        }
    }

    std::unique_ptr<ExportAffinityAPIRequest> RequestForExport() {
        auto request = std::make_unique<ExportAffinityAPIRequest>();
        request->rankId = "0";
        request->orderBy = "duration";
        request->orderType = "descend";
        request->projectName = projectName;
        return request;
    }

    void RegisterProject(const std::string &path) {
        Global::ProjectExplorerInfo info;
        info.projectName = projectName;
        info.fileName = path;
        info.importType = "import";
        info.projectType = static_cast<int64_t>(ProjectTypeEnum::TRACE);
        info.accessTime = "2000-01-01 00:00:00.000";
        // QueryProjectExplorer returns only imports with a parsed-file child.
        auto parsedFile = std::make_shared<Global::ParseFileInfo>();
        parsedFile->parseFilePath = path;
        parsedFile->subId = "advisor_export_ut";
        parsedFile->type = Global::ParseFileType::RANK;
        info.AddSubParseFileInfo(parsedFile);
        ASSERT_TRUE(Global::ProjectExplorerManager::Instance().SaveProjectExplorer(info, false));
    }

    void VerifySizeBoundary() {
        ExportAffinityAPIAdvice handler;
        const auto path = exportDir / "size_boundary.csv";
        handler.ofs = FileUtil::OpenWrite(path.string(), std::ios::out | std::ios::trunc);
        ASSERT_TRUE(handler.ofs.is_open());
        const std::string row = "a,b\n";
        // 设置逻辑计数接近1GB，避免测试实际写入1GB；阈值判断及写入仍运行真实代码。
        handler.currentFileSize = handler.maxFileSize - row.size();
        EXPECT_TRUE(handler.AppendFileContent(row));
        EXPECT_EQ(handler.currentFileSize, handler.maxFileSize);
        EXPECT_FALSE(handler.AppendFileContent(row));
        EXPECT_EQ(handler.currentFileSize, handler.maxFileSize + row.size());
        EXPECT_TRUE(handler.DestroyFile());
        std::ifstream input(path);
        const std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        EXPECT_EQ(bytes, row + row); // 超限时该行已完整写入，与算子导出策略一致。
        EXPECT_FALSE(handler.AppendFileContent(row));
    }

    void VerifyOverlongPathRejected() {
        RegisterProject(exportDir.string());
        ExportAffinityAPIAdvice handler;
        ExportAffinityAPIResBody body;
        EXPECT_FALSE(handler.CreateCsvFile(std::string(FileUtil::GetFilePathLengthLimit(), 'a'), projectName, body));
        EXPECT_TRUE(body.filePath.empty());
        EXPECT_FALSE(handler.ofs.is_open());
    }

    void VerifyUniqueExports() {
        constexpr size_t count = 8;
        const auto rank = ExportAffinityAPIAdvice::SanitizeFileName("/tmp/a_b/x");
        ASSERT_EQ(rank, ExportAffinityAPIAdvice::SanitizeFileName("/tmp/a/b/x"));
        const std::string stem = (exportDir / ("advisor_affinity_api_" + rank + "_1700000000")).string();
        std::vector<std::string> paths(count);
        const auto write = [&](size_t index) {
            ExportAffinityAPIAdvice handler;
            ExportAffinityAPIResBody body;
            if (handler.OpenUniqueCsvFile(stem, body) &&
                handler.AppendFileContent("export " + std::to_string(index) + "\n") && handler.DestroyFile()) {
                paths[index] = body.filePath;
            }
        };
        // 先验证原名及重复导出的序号，再让并发导出与已有文件碰撞；最后统一核对所有内容。
        write(0);
        write(1);
        EXPECT_EQ(paths[0], stem + ".csv");
        EXPECT_EQ(paths[1], stem + "_1.csv");
        std::vector<std::thread> workers;
        for (size_t index = 2; index < count; ++index) {
            workers.emplace_back(write, index);
        }
        for (auto &worker : workers) {
            worker.join();
        }
        EXPECT_EQ(std::set<std::string>(paths.begin(), paths.end()).size(), count);
        for (size_t index = 0; index < count; ++index) {
            ASSERT_FALSE(paths[index].empty());
            auto input = FileUtil::OpenRead(paths[index]);
            EXPECT_EQ((std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()}),
                "export " + std::to_string(index) + "\n");
        }
    }
};

TEST_F(ExportAffinityAPIIntegrationTest, EmptyQuerySucceedsWithoutCreatingFileOrRequiringExportDirectory) {
    auto request = RequestForExport();
    request->startTime = std::numeric_limits<uint64_t>::max() / 2;
    request->endTime = request->startTime + 1;
    APITypeParams params;
    params.rankId = request->rankId;
    params.orderBy = request->orderBy;
    params.orderType = request->orderType;
    params.startTime = request->startTime;
    params.endTime = request->endTime;
    std::vector<AffinityAPIData> rows;
    ASSERT_TRUE(AffinityAPIAdvisor::ProcessAll(params, rows));
    ASSERT_TRUE(rows.empty());
    ExportAffinityAPIAdvice handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    EXPECT_TRUE(std::filesystem::is_empty(exportDir));
}

TEST_F(ExportAffinityAPIIntegrationTest, SizeLimitIsCheckedAfterWritingCompleteRow) { VerifySizeBoundary(); }

TEST_F(ExportAffinityAPIIntegrationTest, OverlongExportPathDoesNotOpenFile) { VerifyOverlongPathRejected(); }

TEST_F(ExportAffinityAPIIntegrationTest, CollidingRepeatedAndConcurrentExportsPreserveAllFiles) {
    VerifyUniqueExports();
}

TEST_F(ExportAffinityAPIIntegrationTest, ExportWritesBomHeadersAndAllMatchingRows) {
    RegisterProject(exportDir.string());
    APITypeParams params;
    params.rankId = "0";
    params.orderBy = "duration";
    params.orderType = "descend";
    std::vector<AffinityAPIData> expected;
    ASSERT_TRUE(AffinityAPIAdvisor::ProcessAll(params, expected));
    ASSERT_FALSE(expected.empty());
    ExportAffinityAPIAdvice handler;
    ASSERT_TRUE(handler.HandleRequest(RequestForExport()));
    std::vector<std::filesystem::path> csvFiles;
    for (const auto &entry : std::filesystem::directory_iterator(exportDir)) {
        if (entry.path().extension() == ".csv") {
            csvFiles.push_back(entry.path());
        }
    }
    ASSERT_EQ(csvFiles.size(), 1);
    std::ifstream input(csvFiles.front(), std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content.substr(0, 3), "\xEF\xBB\xBF");
    EXPECT_NE(content.find("Name,Origin API,Replacement API,Start Time,Duration(ns),Process Id,Thread Id,Notes"),
        std::string::npos);
    EXPECT_NE(content.find(expected.front().name), std::string::npos);
    EXPECT_EQ(std::count(content.begin(), content.end(), '\n'), expected.size() + 1);
    // 每行数据都必须保持8列；多API规则拼接出的逗号不能把后续列挤偏。
    std::istringstream lines(content.substr(3));
    std::string line;
    std::getline(lines, line);
    while (std::getline(lines, line)) {
        EXPECT_EQ(CountCsvFields(line), 8U) << line;
    }
}

TEST_F(ExportAffinityAPIIntegrationTest, ExportFailsWhenProjectIsMissing) {
    ExportAffinityAPIAdvice handler;
    EXPECT_FALSE(handler.HandleRequest(RequestForExport()));
}

TEST_F(ExportAffinityAPIIntegrationTest, JsonFileProjectExportsBesideSourceFile) {
    const auto source = exportDir / "trace_view.json";
    { std::ofstream(source) << "{}"; }
    RegisterProject(source.string());
    ExportAffinityAPIAdvice handler;
    EXPECT_TRUE(handler.HandleRequest(RequestForExport()));
    std::ifstream input(source);
    EXPECT_EQ((std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()}), "{}");
    input.close();
    std::filesystem::remove(source);
    EXPECT_EQ(std::count_if(std::filesystem::directory_iterator(exportDir), std::filesystem::directory_iterator(),
                  [](const auto &entry) { return entry.path().extension() == ".csv"; }),
        1);
}

TEST_F(ExportAffinityAPIIntegrationTest, ExportFailsWhenProjectDirectoryDoesNotExist) {
    RegisterProject((exportDir / "missing").string());
    ExportAffinityAPIAdvice handler;
    EXPECT_FALSE(handler.HandleRequest(RequestForExport()));
}

TEST_F(ExportAffinityAPIIntegrationTest, Utf8ExportDirectoryProducesCsv) {
    Dic::Test::Utf8TempDirectory directory;
    RegisterProject(FileUtil::GetParentPath(directory.Path()));
    ExportAffinityAPIAdvice handler;
    ASSERT_TRUE(handler.HandleRequest(RequestForExport()));
    std::vector<std::filesystem::path> files;
    const auto nativeDirectory = Dic::Test::Utf8TempDirectory::NativePath(directory.Path());
    for (const auto &entry : std::filesystem::directory_iterator(nativeDirectory)) {
        if (entry.path().extension() == ".csv") {
            files.push_back(entry.path());
        }
    }
    ASSERT_EQ(files.size(), 1U);
    std::ifstream stream(files.front(), std::ios::binary);
    ASSERT_TRUE(stream.good());
    const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    EXPECT_EQ(bytes.substr(0, 3), "\xEF\xBB\xBF");
    EXPECT_NE(bytes.find("Name,Origin API,Replacement API,Start Time,Duration(ns),Process Id,Thread Id,Notes"),
        std::string::npos);
}

TEST_F(ExportAffinityAPIIntegrationTest, Utf8OperatorExportDirectoryProducesCsv) {
    Dic::Test::Utf8TempDirectory directory;
    RegisterProject(FileUtil::GetParentPath(directory.Path()));
    auto request = std::make_unique<Dic::Protocol::OperatorExportDetailsRequest>();
    request->projectName = projectName;
    request->params.rankId = "0";
    request->params.group = "Operator Type";
    Dic::Module::Operator::ExportOpDetailsHandler handler;
    ASSERT_TRUE(handler.HandleRequest(std::move(request)));
    std::vector<std::filesystem::path> files;
    const auto nativeDirectory = Dic::Test::Utf8TempDirectory::NativePath(directory.Path());
    for (const auto &entry : std::filesystem::directory_iterator(nativeDirectory)) {
        if (entry.path().extension() == ".csv") {
            files.push_back(entry.path());
        }
    }
    ASSERT_EQ(files.size(), 1U);
    std::ifstream stream(files.front(), std::ios::binary);
    ASSERT_TRUE(stream.good());
    const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    EXPECT_NE(bytes.find("opType"), std::string::npos);
}

} // Dic::Module::Advisor
