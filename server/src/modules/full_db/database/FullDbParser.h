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

#ifndef PROFILER_SERVER_FULLDBPARSER_H
#define PROFILER_SERVER_FULLDBPARSER_H

#include <map>
#include "FileParser.h"
#include "ThreadPool.h"
#include "DbTraceDataBase.h"

namespace Dic::Module::FullDb {
class FullDbParser : public FileParser {
  public:
    static FullDbParser &Instance();
    FullDbParser();

    ~FullDbParser() override;
    bool Parse(const std::vector<std::string> &fileIds, const std::string &filePath, const std::string &selectedFolder,
        const std::string &fileId) override;
    void Reset() override;
    static void InitOpenDb(const std::string &filePath, const std::vector<std::string> &rankId);

    bool Parse(const std::vector<std::string> &rankIds, const std::string &fileId);

  private:
    std::unique_ptr<ThreadPool> threadPool;
    const int maxThreadNum = 4;

    static void InitMemory(const std::vector<std::string> &rankIds, const std::string &path);
    static void InitSummary(const std::vector<std::string> &rankIds, const std::string &path);

    static void ParserCallBack(std::string rankId, const std::string &fileId, bool result);
    static std::shared_ptr<DbTraceDataBase> GetTraceDatabase(const std::string &filePath);
    static void EndParseTask(const std::vector<std::string> &rankIds, const std::string &filePath,
        const std::shared_ptr<std::vector<std::future<void>>> &futures,
        std::chrono::time_point<std::chrono::high_resolution_clock> start);
    static void BuildProfilingInitTask(
        std::shared_ptr<std::vector<std::future<void>>> &futures, std::string &dbId, std::unique_ptr<ThreadPool> &pool);
};
}

#endif // PROFILER_SERVER_FULLDBPARSER_H
