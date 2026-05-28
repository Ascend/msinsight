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

#ifndef PROFILER_SERVER_PARSER_STATUS_MANAGER_H
#define PROFILER_SERVER_PARSER_STATUS_MANAGER_H

#include <map>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include "vector"
#include "condition_variable"
#include "GlobalDefs.h"

namespace Dic {
namespace Module {
namespace Timeline {
enum class ParserStatus {
    UN_KNOW = 0,
    INIT, // 未开始解析
    RUNNING, // 解析中
    FINISH, // 解释完成
    FINISH_ALL, // 解释完成
    TERMINATE, // 终止解析
};

class ParserStatusManager {
  public:
    static ParserStatusManager &Instance();
    ParserStatusManager(const ParserStatusManager &) = delete;
    ParserStatusManager &operator=(const ParserStatusManager &) = delete;
    ParserStatusManager(ParserStatusManager &&) = delete;
    ParserStatusManager &operator=(ParserStatusManager &&) = delete;

    void SetParserStatus(const std::string &fileId, ParserStatus status);
    void ClearParserStatus(const std::string &fileId);
    void ClearAllParserStatus();
    ParserStatus GetParserStatus(const std::string &fileId);
    void EraseClusterParserStatusByKeyWord(const std::string &keyWord);
    bool IsClusterParserFinalState(const std::string &uniqueKey);
    bool SetRunningStatus(const std::string &fileId);
    bool SetFinishStatus(const std::string &fileId);
    void SetAllTerminateStatus();
    // return old status
    ParserStatus SetTerminateStatus(const std::string &fileId);
    bool SetClusterParseStatus(const std::string &uniqueKey, ParserStatus parserStatus);
    void WaitAllFinished(const std::vector<std::string> &fileIds);
    bool IsAllFinished(std::string &notFinishTask);
    bool IsFinished(const std::string &fileId);
    bool IsKernelAndMemoryFinished(const std::string &fileId);
    void SetPendingStatus(
        const std::string &fileId, const std::pair<ProjectTypeEnum, std::vector<std::string>> &filePathPair);
    std::pair<ProjectTypeEnum, std::vector<std::string>> QueryPendingFilePath(const std::string &fileId);

    void WaitStartParse();

    void NotifyStartParse();

    void ResetParse();

  private:
    ParserStatusManager() = default;
    ~ParserStatusManager() = default;

    static bool SetStatusToInit(const std::string &uniqueKey, std::map<std::string, ParserStatus> &statusMap);
    static bool SetStatusToRunning(const std::string &uniqueKey, std::map<std::string, ParserStatus> &statusMap);
    static bool SetStatusToFinalState(
        const std::string &uniqueKey, ParserStatus parserStatus, std::map<std::string, ParserStatus> &statusMap);

    std::shared_mutex mutex;
    std::map<std::string, ParserStatus> statusMap;
    std::map<std::string, std::pair<ProjectTypeEnum, std::vector<std::string>>> pendingRankAndFilePathMap;
    std::map<std::string, ParserStatus> clusterStatusMap;
    std::condition_variable_any parseCv;
    const std::vector<ParserStatus> finalStateList = {
        ParserStatus::FINISH, ParserStatus::FINISH_ALL, ParserStatus::TERMINATE};
    std::condition_variable importResCv;
    std::mutex importMutex;
    std::atomic<bool> importRes{true}; // 默认为true， UT中无需设置
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_PARSER_STATUS_MANAGER_H
