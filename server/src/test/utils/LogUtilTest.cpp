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

#include "Utf8TestUtil.h"
#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include "TimeUtil.h"
#include "FileUtil.h"
#include "LogUtil.h"

namespace Dic {
class LogUtilTestAccess {
  public:
    static void SetBadStream(LogUtil &logger) { logger.ofs.setstate(std::ios::badbit); }
};
}

namespace Dic::Test {
// Count real filesystem opens without replacing FileUtil path/security behavior.
class CountingLogFileUtil : public FileUtil {
  public:
    static inline size_t readOpens = 0;
    static inline size_t writeOpens = 0;
    static std::ifstream OpenRead(const std::string &path, std::ios_base::openmode mode) {
        ++readOpens;
        return FileUtil::OpenRead(path, mode);
    }
    static std::ofstream OpenWrite(const std::string &path, std::ios_base::openmode mode) {
        ++writeOpens;
        return FileUtil::OpenWrite(path, mode);
    }
};
}

using namespace Dic;

namespace {
std::string ReadLogBytes(const std::string &path) {
    std::ifstream stream(Dic::Test::Utf8TempDirectory::NativePath(path), std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

#ifdef _WIN32
constexpr const char *LINE_ENDING = "\r\n";
#else
constexpr const char *LINE_ENDING = "\n";
#endif
}

TEST(LogUtil, PercentFormattingRules) {
    const std::string originPath = FileUtil::SplicePath(::testing::TempDir(), "log_util_percent.log");
    const std::string expectedPath = FileUtil::SplicePath(::testing::TempDir(), "log_util_percent_244_1.log");
    if (FileUtil::CheckFilePathExist(expectedPath)) {
        ASSERT_TRUE(FileUtil::RemoveFile(expectedPath));
    }

    std::string actualPath;
    {
        LogUtil logger(LogOutType::FILE, originPath, "244");
        logger.LogT(LogLevel::L_INFO, "", "progress=100%%, op=%", "Mat%Mul");
        std::vector<std::string> values = {"progress=100%%, rankId=%, op=%", "244", "Mat%Mul"};
        logger.LogT(LogLevel::L_INFO, std::string(), values);
        actualPath = logger.GetLogFilePath();
    }

    std::ifstream stream(actualPath, std::ios::binary);
    ASSERT_TRUE(stream);
    const std::string log{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    EXPECT_NE(log.find("progress=100%, op=Mat%Mul"), std::string::npos);
    EXPECT_NE(log.find("progress=100%, rankId=244, op=Mat%Mul"), std::string::npos);
    stream.close();
    EXPECT_TRUE(FileUtil::RemoveFile(actualPath));
}

TEST(LogUtil, Utf8LogPathAndContentSurviveRotationAndFlush) {
    Dic::Test::Utf8TempDirectory directory;
    std::string first;
    std::string second;
    {
        LogUtil logger(LogOutType::FILE, directory.Path(u8"练😀.log"), "utf8", 1);
        logger.LogT(LogLevel::L_INFO, "", u8"练 😀 first");
        first = logger.GetLogFilePath();
        logger.LogT(LogLevel::L_INFO, "", u8"练 😀 second");
        second = logger.GetLogFilePath();
    }
    EXPECT_NE(first, second);
    for (const auto &path : {first, second}) {
        std::ifstream stream(Dic::Test::Utf8TempDirectory::NativePath(path), std::ios::binary);
        ASSERT_TRUE(stream.good());
        const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        EXPECT_NE(bytes.find(u8"练 😀"), std::string::npos);
    }
}

#ifdef _WIN32
TEST(LogUtil, NormalFileWritesDoNotReopenForEveryRecord) {
    Dic::Test::Utf8TempDirectory directory;
    Dic::Test::CountingLogFileUtil::readOpens = 0;
    Dic::Test::CountingLogFileUtil::writeOpens = 0;
    LogUtil logger(LogOutType::FILE, directory.Path("persistent.log"), "opens", 10 * 1024 * 1024,
        {&Dic::Test::CountingLogFileUtil::OpenRead, &Dic::Test::CountingLogFileUtil::OpenWrite});
    ASSERT_GT(Dic::Test::CountingLogFileUtil::readOpens, 0);
    ASSERT_GT(Dic::Test::CountingLogFileUtil::writeOpens, 0);
    const auto initialReadOpens = Dic::Test::CountingLogFileUtil::readOpens;
    const auto initialWriteOpens = Dic::Test::CountingLogFileUtil::writeOpens;
    for (size_t i = 0; i < 100; ++i) {
        logger.LogT(LogLevel::L_INFO, "", "record ", i);
    }
    EXPECT_EQ(Dic::Test::CountingLogFileUtil::readOpens, initialReadOpens);
    EXPECT_EQ(Dic::Test::CountingLogFileUtil::writeOpens, initialWriteOpens);
    const auto bytes = ReadLogBytes(logger.GetLogFilePath());
    EXPECT_NE(bytes.find("record 0"), std::string::npos);
    EXPECT_NE(bytes.find("record 99"), std::string::npos);
}
#endif

TEST(LogUtil, EveryRecordRemainsVisibleBeforeTheLoggerIsDestroyed) {
    Dic::Test::Utf8TempDirectory directory;
    LogUtil logger(LogOutType::FILE, directory.Path(u8"可见😀.log"), "visible");
    std::string expected;
    for (const auto &line : {"first record", "second record", "third record"}) {
        logger.LogT(LogLevel::L_INFO, "", line);
        expected += std::string(line) + LINE_ENDING;
        EXPECT_EQ(ReadLogBytes(logger.GetLogFilePath()), expected);
    }
}

TEST(LogUtil, RotationUsesPhysicalFileBytesIncludingLineEndings) {
    Dic::Test::Utf8TempDirectory directory;
    const int firstRecordBytes = 3 + static_cast<int>(std::char_traits<char>::length(LINE_ENDING));
    LogUtil logger(LogOutType::FILE, directory.Path("size.log"), "size", firstRecordBytes);
    logger.LogT(LogLevel::L_INFO, "", "abc");
    const auto first = logger.GetLogFilePath();
    logger.LogT(LogLevel::L_INFO, "", "def");
    const auto second = logger.GetLogFilePath();
    EXPECT_NE(first, second);
    EXPECT_EQ(ReadLogBytes(first), std::string("abc") + LINE_ENDING);
    EXPECT_EQ(ReadLogBytes(second), std::string("def") + LINE_ENDING);
}

TEST(LogUtil, ExistingFullFileIsSkippedAndTenFileWrapStillTruncatesOldRecords) {
    Dic::Test::Utf8TempDirectory directory;
    const auto origin = directory.Path("ring.log");
    {
        auto initial = FileUtil::OpenWrite(directory.Path("ring_ring_1.log"), std::ios::out | std::ios::trunc);
        initial << "old full record" << std::endl;
    }
    LogUtil logger(LogOutType::FILE, origin, "ring", 1);
    EXPECT_NE(logger.GetLogFilePath(), directory.Path("ring_ring_1.log"));
    for (size_t i = 0; i < 25; ++i) {
        logger.LogT(LogLevel::L_INFO, "", "record ", i);
    }
    size_t fileCount = 0;
    for (const auto &entry :
        std::filesystem::directory_iterator(Dic::Test::Utf8TempDirectory::NativePath(directory.Path("")))) {
        if (entry.is_regular_file()) {
            ++fileCount;
        }
    }
    EXPECT_EQ(fileCount, 10);
    const auto current = ReadLogBytes(logger.GetLogFilePath());
    EXPECT_EQ(current, std::string("record 24") + LINE_ENDING);
}

TEST(LogUtil, AFailedStreamCanReopenWithoutDroppingTheNextRecord) {
    Dic::Test::Utf8TempDirectory directory;
    LogUtil logger(LogOutType::FILE, directory.Path("recover.log"), "recover", 10 * 1024 * 1024,
        {&Dic::Test::CountingLogFileUtil::OpenRead, &Dic::Test::CountingLogFileUtil::OpenWrite});
    logger.LogT(LogLevel::L_INFO, "", "before failure");
    Dic::Test::CountingLogFileUtil::writeOpens = 0;
    LogUtilTestAccess::SetBadStream(logger);
    logger.LogT(LogLevel::L_INFO, "", "after recovery");
    EXPECT_EQ(Dic::Test::CountingLogFileUtil::writeOpens, 1);
    EXPECT_EQ(ReadLogBytes(logger.GetLogFilePath()),
        std::string("before failure") + LINE_ENDING + "after recovery" + LINE_ENDING);
}

TEST(LogUtil, CallerSerializedConcurrentWritersKeepEachRecordExactlyOnce) {
    Dic::Test::Utf8TempDirectory directory;
    LogUtil logger(LogOutType::FILE, directory.Path("workers.log"), "workers");
    std::mutex mutex;
    std::vector<std::thread> workers;
    for (size_t worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&, worker]() {
            for (size_t row = 0; row < 25; ++row) {
                std::lock_guard<std::mutex> lock(mutex); // Same ownership contract as ServerLog.
                logger.LogT(LogLevel::L_INFO, "", worker, ":", row);
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }
    std::istringstream stream(ReadLogBytes(logger.GetLogFilePath()));
    std::set<std::string> records;
    std::string line;
    size_t count = 0;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        records.insert(line);
        ++count;
    }
    EXPECT_EQ(count, 100);
    EXPECT_EQ(records.size(), 100);
}
