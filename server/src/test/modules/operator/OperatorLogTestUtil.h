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

#ifndef PROFILER_SERVER_OPERATOR_LOG_TEST_UTIL_H
#define PROFILER_SERVER_OPERATOR_LOG_TEST_UTIL_H

#include <gtest/gtest.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "ServerLog.h"
#include "WsSession.h"

namespace OperatorLogTestUtil {
class CapturingWsSession : public Dic::Server::WsSession {
  public:
    void Start() override { status = Status::STARTED; }
    Status GetStatus() const override { return status; }
    void OnResponse(std::unique_ptr<Dic::Protocol::Response> responsePtr) override {
        response = std::move(responsePtr);
    }
    void SendBaseResponse(std::unique_ptr<Dic::Protocol::Response> responsePtr) override {
        response = std::move(responsePtr);
    }
    void OnEvent(std::unique_ptr<Dic::Protocol::Event>) override {}
    void SetStatus(Status sessionStatus) override { status = sessionStatus; }
    void WaitForExit(int = 10000) override {}
    void Reset() { response.reset(); }
    template <typename ResponseType> const ResponseType *GetResponse() const {
        return dynamic_cast<const ResponseType *>(response.get());
    }

  private:
    Status status = Status::INIT;
    std::unique_ptr<Dic::Protocol::Response> response;
};

struct LogMark {
    std::string path;
    std::streamoff offset = 0;
};

inline LogMark Mark() {
    const std::string path = Dic::Server::ServerLog::GetCurrentLogPath();
    EXPECT_FALSE(path.empty()) << "The global test environment must initialize ServerLog before log assertions";
    LogMark mark{path};
    std::ifstream log(mark.path, std::ios::binary | std::ios::ate);
    EXPECT_TRUE(log) << "Failed to open log file: " << mark.path;
    mark.offset = log ? static_cast<std::streamoff>(log.tellg()) : 0;
    return mark;
}

inline std::string ReadSince(const LogMark &mark) {
    std::ifstream stream(mark.path, std::ios::binary);
    EXPECT_TRUE(stream) << "Failed to open log file: " << mark.path;
    if (!stream) {
        return {};
    }
    stream.seekg(mark.offset, std::ios::beg);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

inline std::string::size_type Count(const std::string &text, const std::string &token) {
    std::string::size_type count = 0, pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        ++count;
        pos += token.size();
    }
    return count;
}

inline void ExpectSingleOperatorLog(const LogMark &mark, std::string::size_type expectedWarnCount,
    std::string::size_type expectedErrorCount, const std::string &operation, const std::string &stage,
    const std::vector<std::string> &requiredTokens, const std::vector<std::string> &forbiddenTokens) {
    const std::string log = ReadSince(mark);
    const std::string marker = "[Operator][" + operation + "][" + stage + "]";
    EXPECT_EQ(Count(log, "[Warn]"), expectedWarnCount) << "Unexpected global WARN count";
    EXPECT_EQ(Count(log, "[Error]"), expectedErrorCount) << "Unexpected global ERROR count";
    EXPECT_EQ(Count(log, "[Operator]"), 1) << "Expected exactly one Operator log";
    EXPECT_EQ(Count(log, marker), 1) << "Expected exactly one log for " << marker;
    const auto markerPos = log.find(marker);
    ASSERT_NE(markerPos, std::string::npos);
    const auto lineBegin = log.rfind('\n', markerPos);
    const auto lineEnd = log.find('\n', markerPos);
    const std::string line = log.substr(lineBegin == std::string::npos ? 0 : lineBegin + 1,
        lineEnd == std::string::npos ? std::string::npos : lineEnd - lineBegin - 1);
    const auto causePos = line.find("cause=");
    const auto suggestionPos = line.find(", suggestion=");
    ASSERT_NE(causePos, std::string::npos);
    ASSERT_NE(suggestionPos, std::string::npos);
    EXPECT_GT(suggestionPos, causePos + std::string("cause=").size());
    EXPECT_GT(line.size(), suggestionPos + std::string(", suggestion=").size() + 1);
    for (const auto &token : requiredTokens) {
        EXPECT_NE(line.find(token), std::string::npos) << "Missing required log token: " << token;
    }
    for (const auto &token : forbiddenTokens) {
        EXPECT_EQ(log.find(token), std::string::npos) << "Forbidden log token: " << token;
    }
}
} // namespace OperatorLogTestUtil
#endif // PROFILER_SERVER_OPERATOR_LOG_TEST_UTIL_H
