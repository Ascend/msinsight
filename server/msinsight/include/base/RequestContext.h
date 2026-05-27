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

#ifndef PROFILER_SERVER_REQUESTCONTEXT_H
#define PROFILER_SERVER_REQUESTCONTEXT_H
#include <memory>
#include <string>
#include <iostream>

namespace Dic {
namespace Server {
class RequestContext {
  public:
    // 获取当前线程的请求上下文
    static RequestContext &GetInstance() {
        static thread_local RequestContext instance;
        return instance;
    }

    void ResetError() { error_ = {}; }

    void SetError(Protocol::ErrorMessage error) { error_ = error; }

    // 获取响应信息
    Protocol::ErrorMessage GetError() { return error_; }

  private:
    RequestContext() = default;
    ~RequestContext() = default;

    // 禁止拷贝和赋值
    RequestContext(const RequestContext &) = delete;
    RequestContext &operator=(const RequestContext &) = delete;

    Protocol::ErrorMessage error_;
};
} // namespace Server
} // namespace Dic

#endif // PROFILER_SERVER_REQUESTCONTEXT_H
