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

#ifndef PROFILER_SERVER_MEMSNAPSHOTPARSER_H
#define PROFILER_SERVER_MEMSNAPSHOTPARSER_H
#include "pch.h"
#include "TimelineProtocolEvent.h"
#include "MemScopeProtocolEvent.h"
#include "MemSnapshotProtocolEvent.h"
#include "ThreadPool.h"

namespace Dic::Module {
enum class ParserState {
    INIT = -1,
    Loading = 0,
    Processing = 1,
    FINISH_SUCCESS = 2,
    FINISH_FAILURE = 3,
    UP_TO_DATE = 4,
};

struct MemSnapshotParserContext {
  public:
    MemSnapshotParserContext() = default;
    void Reset(std::string nPicklePath = "", std::string nLogPath = "", std::string nOutputPath = "");
    bool IsFinished() const;
    bool IsReadyToParse() const;
    std::string GetPicklePath() const;
    std::string GetLogPath() const;
    std::string GetOutputDbPath() const;
    ParserState GetState() const;
    void SetState(const ParserState &newState);
    uint8_t GetProgress() const;
    void SetProgress(uint8_t newProgress);
    std::string GetWorkDir() const;
    static void SendParseProgressEvent(int progress);

  private:
    std::string picklePath{};
    std::string logPath{};
    std::string outputDbPath{};
    ParserState state{ParserState::INIT};
    std::string workDir{};
    uint8_t progress{0}; // 百分制进度

    mutable std::shared_mutex _mutex{};
};

class MemSnapshotParser {
  public:
    MemSnapshotParser(const MemSnapshotParser &) = delete;
    MemSnapshotParser &operator=(const MemSnapshotParser &) = delete;
    static MemSnapshotParser &Instance();
    void Reset();
    // 异步解析Pickle文件API
    void AsyncParseMemSnapshotPickle(const std::string &pickleFilePath);
    MemSnapshotParserContext &GetParseContext();
    // 用于检查是否需要解析或重新解析pickle文件
    static bool CheckIfParsingNeed(const MemSnapshotParserContext &context);

  private:
    MemSnapshotParser();
    ~MemSnapshotParser();
    // 解析线程，将解析结果重定向到logPath日志文件
    static void ParseMemSnapshotTask();
    // 守护线程，实施读取解析进展
    static void ParseDaemonTask();
    static void ParseCallBack();
    // 解析完成回调方法之一，用于二次检查db结果并设置解析结果db版本
    bool TryOpenParsingResultDbAndSetVersion() const;
    // 构造解析响应事件
    std::unique_ptr<MemScopeParseSuccessEvent> BuildParseSuccessEventFromContext() const;
    std::unique_ptr<ParseFailEvent> BuildParseFailEventFromContext(const std::string &errMsg) const;
    std::unique_ptr<ThreadPool> _threadPool;
    MemSnapshotParserContext parseContext = MemSnapshotParserContext();
};
}
#endif //PROFILER_SERVER_MEMSNAPSHOTPARSER_H
