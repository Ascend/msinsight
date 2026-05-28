// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
//  *
//  * MindStudio is licensed under Mulan PSL v2.
//  * You can use this software according to the terms and conditions of the Mulan PSL v2.
//  * You may obtain a copy of Mulan PSL v2 at:
//  *
//  *          http://license.coscl.org.cn/MulanPSL2
//  *
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//  * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//  * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//  * See the Mulan PSL v2 for more details.
//  * -------------------------------------------------------------------------
//  *

#ifndef PROFILER_SERVER_TRITONPARSER_H
#define PROFILER_SERVER_TRITONPARSER_H
#include <future>
#include <string>
#include <vector>
#include <utility>
#include <atomic>
#include "JsonUtil.h"
namespace Dic::Module::Triton {
struct ParseProgressInfo {
    uint64_t parsedSize{0};
    uint64_t totalSize{0};
};

class ParseResult {
  public:
    ParseResult(bool success, std::string msg) : success(success), msg(std::move(msg)) {}
    [[nodiscard]] bool IsSuccess() const { return success; }
    [[nodiscard]] std::string GetErrorMsg() const { return msg; }
    explicit operator bool() const noexcept { return success; }
    bool operator!() const noexcept { return !success; }
    void Set(const bool result, std::string message) {
        success = result;
        msg = std::move(message);
    }

  private:
    bool success{false};
    std::string msg;
};

class TritonParser {
  public:
    static TritonParser &Instance();
    void Parse(const std::string &parseDir);
    bool IsParsed(const std::string &filePath) const;

  protected:
    void BeforeParse(const std::string &parsedDir);
    void AfterParse(const ParseResult &result) const;
    ParseResult ParseImpl(const std::string &parsedDir);
    bool CheckFileValid(const std::string &fileName, std::string &error);
    ParseResult ParseOneTriton(const std::string &memFile);
    bool CheckDataValid(document_t &json);

  private:
    std::vector<std::string> parsedFiles;
    std::atomic<bool> terminated{false};
    std::string_view tritonMemFileName = "memory_info.json";
};
}

#endif //PROFILER_SERVER_TRITONPARSER_H
