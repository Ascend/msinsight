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
#ifndef PROFILER_SERVER_WSSESSION_H
#define PROFILER_SERVER_WSSESSION_H
#include "ProtocolUtil.h"
namespace Dic::Server {
class WsSession {
  public:
    enum class Status { INIT, STARTED, CLOSED };
    virtual ~WsSession() = default;
    virtual void Start() = 0;
    virtual Status GetStatus() const = 0;
    virtual void OnResponse(std::unique_ptr<Protocol::Response> responsePtr) = 0;
    virtual void SendBaseResponse(std::unique_ptr<Protocol::Response> responsePtr) = 0;
    virtual void OnEvent(std::unique_ptr<Protocol::Event> eventPtr) = 0;
    virtual void SetStatus(Status sessionStatus) = 0;
    virtual void WaitForExit(int milliSeconds = 10000) = 0;
};
}
#endif // PROFILER_SERVER_WSSESSION_H
