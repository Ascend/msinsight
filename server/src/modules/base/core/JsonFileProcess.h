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
#ifndef PROFILER_SERVER_JSONFILEPROCESS_H
#define PROFILER_SERVER_JSONFILEPROCESS_H
#include <functional>
#include <string>

namespace Dic {
namespace Module {
class JsonFileProcess {
  public:
    static std::vector<std::pair<int64_t, int64_t>> SplitFile(
        const std::string &filePath, std::optional<std::pair<int64_t, int64_t>> position = std::nullopt);

  protected:
    static const int64_t blockSize = 1024 * 1024 * 50; // 50MB
    static const int startBufferLength = 1024;
    static const int endBufferLength = 1024 * 1024;
    static std::vector<std::pair<int64_t, int64_t>> GetSplitPosition(
        std::ifstream &file, std::pair<int64_t, int64_t> position = {0, 0});

  private:
    static bool SeekCharPosition(std::ifstream &file, char c);
    static bool SeekRegexPosition(std::ifstream &file, const std::string &regex);
    static void ComputeSmallFilePosition(std::ifstream &file, std::vector<std::pair<int64_t, int64_t>> &result,
        const JsonFormat &json, std::pair<int64_t, int64_t> position);

    static bool SeekPhEndPosition(std::ifstream &file, bool endFlag, int bufferLength);
};
} // end of namespace Module
} // end of namespace Dic
#endif // PROFILER_SERVER_JSONFILEPROCESS_H
