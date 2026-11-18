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
#include <vector>
#include <string>
#include <optional>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>
#include "gtest/gtest.h"
#include "FileUtil.h"
#include "PythonUtil.h"
#include "ParamsParser.h"

using namespace Dic;

namespace {
class ScopedStderrCapture {
  public:
    ScopedStderrCapture() : previous(std::cerr.rdbuf(output.rdbuf())) {}
    ~ScopedStderrCapture() { std::cerr.rdbuf(previous); }
    std::string Str() const { return output.str(); }

  private:
    std::ostringstream output;
    std::streambuf *previous;
};

class ScopedCurrentDirectory {
  public:
    explicit ScopedCurrentDirectory(const std::filesystem::path &path) : previous(std::filesystem::current_path()) {
        std::filesystem::current_path(path);
    }
    ~ScopedCurrentDirectory() {
        std::error_code error;
        std::filesystem::current_path(previous, error);
    }

  private:
    std::filesystem::path previous;
};
} // namespace

class PythonUtilTest : public testing::Test {
  public:
    ~PythonUtilTest() override {
        auto &parser = Dic::Server::ParamsParser::Instance();
        const_cast<Dic::Server::ParamsOption &>(parser.GetOption()) = std::move(originalOptions);
        const_cast<std::string &>(parser.GetError()) = std::move(originalError);
        if (temporaryCreated) {
            std::error_code error;
            std::filesystem::remove_all(temporary, error);
        }
    }

  protected:
    void SetUp() override {
        temporary = std::filesystem::temp_directory_path() /
            ("insight-python-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        temporaryCreated = std::filesystem::create_directory(temporary);
        ASSERT_TRUE(temporaryCreated);
    }

    std::filesystem::path CreateMacResources() {
        const auto resources = temporary / "With spaces.app" / "Contents" / "Resources";
        std::filesystem::create_directories(resources / "profiler" / "server");
        const auto python = resources / "python" / "bin" / "python3";
        std::filesystem::create_directories(python.parent_path());
        std::ofstream(python.string()).close();
        return resources;
    }

    void ExpectResolutionFailure(const std::string &server, const std::string &root, const std::string &reason) {
        ScopedStderrCapture diagnostic;
        EXPECT_TRUE(PythonUtil::ResolveMacPythonCommand(server, root).empty());
        EXPECT_NE(diagnostic.Str().find(reason), std::string::npos);
        EXPECT_NE(diagnostic.Str().find("Resource root: " + (root.empty() ? "<auto>" : root)), std::string::npos);
        EXPECT_NE(diagnostic.Str().find("server directory: " + server), std::string::npos);
    }

    std::filesystem::path temporary;

  private:
    Dic::Server::ParamsOption originalOptions = Dic::Server::ParamsParser::Instance().GetOption();
    std::string originalError = Dic::Server::ParamsParser::Instance().GetError();
    bool temporaryCreated{false};
};

TEST_F(PythonUtilTest, ResolveMacPythonUsesPhysicalResourceRootWithoutCompatibilityLinks) {
    namespace fs = std::filesystem;
    const auto resources = CreateMacResources();
    const auto server = resources / "profiler" / "server";
    const auto python = resources / "python" / "bin" / "python3";
    const auto expected = fs::canonical(resources) / "python" / "bin" / "python3";
    EXPECT_EQ(PythonUtil::ResolveMacPythonCommand(server.string(), ""), expected.string());
    EXPECT_EQ(PythonUtil::ResolveMacPythonCommand("unused", resources.string()), expected.string());
    fs::remove(python);
    ExpectResolutionFailure(server.string(), resources.string(), "bundled interpreter is missing");
}

TEST_F(PythonUtilTest, ResolveMacPythonReportsInvalidRootsAndLayouts) {
    namespace fs = std::filesystem;
    const auto resources = CreateMacResources();
    const auto server = resources / "profiler" / "server";
    const auto regularFile = temporary / "not-a-directory";
    std::ofstream(regularFile.string()).close();
    ExpectResolutionFailure(server.string(), "relative/resources", "resource root must be an absolute path");
    ExpectResolutionFailure(server.string(), (temporary / "missing").string(), "cannot resolve resource root");
    ExpectResolutionFailure(server.string(), regularFile.string(), "resource root is not an accessible directory");
    ExpectResolutionFailure((temporary / "missing").string(), "", "cannot resolve server directory");
    ExpectResolutionFailure(regularFile.string(), "", "server path is not an accessible directory");
    ExpectResolutionFailure(temporary.string(), "", "expected server directory in Contents/Resources/profiler/server");
    const auto misplacedServer = temporary / "wrong-root" / "profiler" / "server";
    fs::create_directories(misplacedServer);
    ExpectResolutionFailure(misplacedServer.string(), "", "expected resource root at Contents/Resources");
}

TEST_F(PythonUtilTest, RuntimeProbeMissingInterpreterDoesNotCreateLogFile) {
    namespace fs = std::filesystem;
    const auto resources = CreateMacResources();
    fs::remove(resources / "python" / "bin" / "python3");
    ASSERT_TRUE(Dic::Server::ParamsParser::Instance().Parse({"exe", "--checkPython"}));
    ScopedCurrentDirectory workingDirectory(temporary);
    ExpectResolutionFailure((resources / "profiler" / "server").string(), resources.string(),
        "bundled interpreter is missing");
    EXPECT_FALSE(fs::exists(temporary / "profiler_server.log"));
}

#ifdef __APPLE__
TEST_F(PythonUtilTest, GetPythonCommandMatchesMacBuildMode) {
    namespace fs = std::filesystem;
    const auto resources = CreateMacResources();
    auto &parser = Dic::Server::ParamsParser::Instance();
    EXPECT_FALSE(parser.Parse({"exe", "--resourceRoot=relative/resources"}));
    ASSERT_TRUE(parser.Parse({"exe", "--resourceRoot=" + resources.string(), "--checkPython"}));
    EXPECT_TRUE(parser.GetOption().checkPython);
#if defined(INSIGHT_DEBUG)
    EXPECT_EQ(PythonUtil::GetPythonCommand(), "python3");
#else
    const auto expected = fs::canonical(resources) / "python" / "bin" / "python3";
    EXPECT_EQ(PythonUtil::GetPythonCommand(), expected.string());
    std::ofstream(expected.string()) << "#!/bin/sh\nexit 0\n";
    fs::permissions(expected, fs::perms::owner_read | fs::perms::owner_exec);
    EXPECT_EQ(PythonUtil::ExecuteCommand(PythonUtil::GetPythonCommand(), {"-c", "pass"}, std::nullopt), 0);
#endif
}
#endif

#if defined(__linux__) || defined(__APPLE__)
TEST_F(PythonUtilTest, RuntimeProbeReportsSpawnFailureWithoutCreatingLogFile) {
    ASSERT_TRUE(Dic::Server::ParamsParser::Instance().Parse({"exe", "--checkPython"}));
    ScopedCurrentDirectory workingDirectory(temporary);
    ScopedStderrCapture diagnostic;
    const auto missing = temporary / "missing-python";
    EXPECT_EQ(PythonUtil::ExecuteCommand(missing.string(), {"-c", "pass"}, std::nullopt), -1);
    EXPECT_NE(diagnostic.Str().find("Failed to spawn executable " + missing.string()), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(temporary / "profiler_server.log"));
}

TEST_F(PythonUtilTest, RuntimeProbeSuccessDoesNotCreateLogFile) {
    namespace fs = std::filesystem;
    const auto python = temporary / "python-probe";
    std::ofstream(python.string()) << "#!/bin/sh\nexit 0\n";
    fs::permissions(python, fs::perms::owner_read | fs::perms::owner_exec);
    ASSERT_TRUE(Dic::Server::ParamsParser::Instance().Parse({"exe", "--checkPython"}));
    ScopedCurrentDirectory workingDirectory(temporary);
    EXPECT_EQ(PythonUtil::ExecuteCommand(python.string(), {"-c", "pass"}, std::nullopt), 0);
    EXPECT_FALSE(fs::exists(temporary / "profiler_server.log"));
}

TEST_F(PythonUtilTest, ExecuteCommandListOnLinuxOrMac) {
    std::string executablePath{"ls"};
    std::vector<std::string> arguments{"-l"};
    int result = PythonUtil::ExecuteCommand(executablePath, arguments, std::nullopt);
    EXPECT_EQ(result, 0);
}
#endif

#ifdef _WIN32
TEST_F(PythonUtilTest, GetPythonCommandReturnsBundledPythonPathOnWindows) {
    std::string expected = FileUtil::SplicePath(FileUtil::GetCurrPath(), "python", "python.exe");

    EXPECT_EQ(PythonUtil::GetPythonCommand(), expected);
}

TEST_F(PythonUtilTest, ExecuteCommandReturnsFailureWhenExecutableMissingOnWindows) {
    std::string executablePath =
        FileUtil::SplicePath(FileUtil::GetCurrPath(), "missing_python_for_python_util_test.exe");

    EXPECT_EQ(PythonUtil::ExecuteCommand(executablePath, {}, std::nullopt), -1);
}
#endif
