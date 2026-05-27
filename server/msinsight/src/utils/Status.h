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
#ifndef PROFILER_SERVER_ERRDEF_H
#define PROFILER_SERVER_ERRDEF_H
#include <string>
namespace Dic {
class Status {
  public:
    Status() = default;
    Status(bool s, std::string errMsg) : isOk(s), errMsg(std::move(errMsg)) {};
    Status(bool s, std::string &&errMsg) : isOk(s), errMsg(std::move(errMsg)) {};
    explicit Status(bool s) : isOk(s) {};
    inline bool Ok() const { return isOk; }
    inline void SetOk() { isOk = true; }
    inline void SetErr() { isOk = false; }
    inline void SetErr(const std::string &msg) {
        isOk = false;
        errMsg = msg;
    }

    inline operator bool() const { return isOk; }

  private:
    bool isOk{true};
    std::string errMsg;
};
}
#endif // PROFILER_SERVER_ERRDEF_H
