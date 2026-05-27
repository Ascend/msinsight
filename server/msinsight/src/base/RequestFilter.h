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

#ifndef PROFILER_SERVER_REQUESTFILTER_H
#define PROFILER_SERVER_REQUESTFILTER_H
#include <unordered_map>
#include <string>
#include "mutex"
#include "../utils/SpinLockGuard.h"
namespace Dic::Server {
class RequestFilter {
  public:
    static RequestFilter &Instance();
    RequestFilter(const RequestFilter &) = delete;
    RequestFilter &operator=(const RequestFilter &) = delete;
    RequestFilter(RequestFilter &&) = delete;
    RequestFilter &operator=(RequestFilter &&) = delete;
    void SetRequestId(const std::string &key, unsigned int id);
    bool IsNeedFilter(const std::string &key, unsigned int id);
    void ClearKey(const std::string &key, unsigned int id);

  private:
    RequestFilter() = default;
    ~RequestFilter() = default;
    std::unordered_map<std::string, unsigned int> filterMap;
    SpinLock mutex;
};
}

#endif // PROFILER_SERVER_REQUESTFILTER_H
