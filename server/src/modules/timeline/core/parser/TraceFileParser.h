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

#ifndef DATA_INSIGHT_CORE_MODULE_CORE_TRACE_FILE_PARSER_H
#define DATA_INSIGHT_CORE_MODULE_CORE_TRACE_FILE_PARSER_H

#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <mutex>
#include "FileParser.h"
#include "ThreadPool.h"
#include "JsonFileProcess.h"
#include "TextTraceDatabase.h"

namespace Dic {
namespace Module {
namespace Timeline {
class TraceFileParser : public FileParser, protected JsonFileProcess {
  public:
    // 构造时注入共享线程池
    explicit TraceFileParser(std::shared_ptr<ThreadPool> threadPool) : threadPool_(std::move(threadPool)) {
        if (!threadPool_) {
            Server::ServerLog::Error("ThreadPool cannot be null");
        }
    }
    ~TraceFileParser() override = default;
    bool Parse(const std::vector<std::string> &filePathArr, const std::string &rankId,
        const std::string &selectedFolder, const std::string &fileId) override;
    void Reset() override;
    static void DeleteParseFiles(const std::vector<std::string> &rankIds);
    void ParseEndCallBack(
        const std::string &rankId, const std::string &fileId, bool result, const std::string &message);

    int64_t GetTrackId(const std::string &fileId, const std::string &pid, const std::string &tid);
    // 禁止拷贝
    TraceFileParser(const TraceFileParser &) = delete;
    TraceFileParser &operator=(const TraceFileParser &) = delete;

  protected:
    // 后处理 Hook 函数，返回 false 表示后处理失败
    virtual bool PostParse(std::shared_ptr<TextTraceDatabase> db, const std::string &rankId);

    /**
     * @brief 根据 database 中的 process 表的 label 列更新当前映射的 RankId -> DeviceId 的 DeviceId 的值
     * @param db 使用到的 database 共享指针
     * @param rankId 映射的 Key
     */
    static void UpdateRankIdDeviceIdMapByProcessData(std::shared_ptr<TextTraceDatabase> db, const std::string &rankId);

  private:
    std::shared_ptr<ThreadPool> threadPool_; // 共享线程池
    static bool CheckInitParser(const std::string &fileId);
    bool InitParser(const std::vector<std::string> &filePathArr, const std::string &rankId, const std::string &fileId);
    void PreParseTask(
        const std::vector<std::string> &filePathArr, const std::string &rankId, const std::string &fileId);
    void ParseTask(const std::string &filePath, const std::string &rankId, std::pair<int64_t, int64_t> pos);
    void EndParseTask(const std::string &rankId, const std::vector<std::string> &filePathArr,
        std::shared_ptr<std::vector<std::future<void>>> futures,
        std::chrono::time_point<std::chrono::high_resolution_clock> start);

    std::mutex trackMutex;
    std::unordered_map<std::string, std::map<std::pair<std::string, std::string>, uint64_t>> trackIdMap;
    uint64_t trackId = 0;

    void InitFileProcess(const std::vector<std::string> &filePathArr, const std::string &rankId);

    static std::string ComputeStatusInfoFromPathArr(const std::vector<std::string> &filePathArr);
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_MODULE_CORE_TRACE_FILE_PARSER_H
