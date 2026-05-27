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
#include "pch.h"
#include "TimeUtil.h"
#include "ModuleManager.h"
#include "ProtocolManager.h"
#include "TraceIdManager.h"
#include "WsSessionImpl.h"

namespace Dic {
namespace Server {
using namespace Dic::Protocol;
using namespace Dic::Module;
WsSessionImpl::WsSessionImpl(WsChannel *channel) : channel(channel), status(Status::INIT) {
    loop = uWS::Loop::get();
    createTime = TimeUtil::Instance().NowUTC();
    msgBuffer = std::make_unique<ProtocolMessageBuffer>();
}

void WsSessionImpl::OnHandleMsgBuffer(WsSessionImpl &session) {
    ServerLog::Info("Handle message buffer thread start.");
    const int interval = 10;
    while (true) {
        if (session.status == Status::CLOSED) {
            session.msgBuffer->Clear();
            break;
        }
        BatchHandleMsg(session);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
    ServerLog::Info("Handle message buffer thread has exited.");
    session.OnNotifyExit();
}

void WsSessionImpl::BatchHandleMsg(WsSessionImpl &session) {
    const uint8_t batchSize = 20;
    for (uint8_t i = 0; i < batchSize; i++) {
        std::unique_ptr<ProtocolMessage> msg = session.msgBuffer->Pop();
        if (msg == nullptr) {
            continue;
        }
        if (msg->type == ProtocolMessage::Type::REQUEST) {
            Request *reqPtr = dynamic_cast<Request *>(msg.release());
            if (reqPtr != nullptr) {
                ModuleManager::Instance().OnDispatchModuleRequest(std::unique_ptr<Request>(reqPtr));
            } else {
                ServerLog::Info("Request is not supported");
            }
        }
    }
}

void WsSessionImpl::OnHandleResponseQueue(WsSessionImpl &session) {
    ServerLog::Info("Handle response queue thread start.");
    while (session.useResponseQueue) {
        if (session.status == Status::CLOSED) {
            session.responseQueue.Clear();
            break;
        }
        std::unique_ptr<Response> responsePtr = nullptr;
        if (session.responseQueue.Pop(responsePtr) && responsePtr == nullptr) {
            std::unique_lock<std::mutex> lock(session.responseQueueMutex);
            session.responseQueueCv.wait(lock);
            continue;
        }
        session.SendResponse(*responsePtr.get());
    }
    ServerLog::Info("Handle response queue thread has exited.");
}

void WsSessionImpl::OnNotifyExit() {
    std::unique_lock<std::mutex> lock(onExitMutex);
    onExitCv.notify_all();
}

void WsSessionImpl::WaitForExit(int milliSeconds) {
    std::unique_lock<std::mutex> lock(onExitMutex);
    onExitCv.wait_for(lock, std::chrono::milliseconds(milliSeconds));
}

const WsChannel *WsSessionImpl::GetChannel() const { return channel; }

WsSessionImpl::Status WsSessionImpl::GetStatus() const { return status; }

void WsSessionImpl::SetStatus(Status sessionStatus) {
    if (this->status != sessionStatus) {
        this->status = sessionStatus;
    }
}

uint32_t WsSessionImpl::GetCreateTime() const { return createTime; }

uint32_t WsSessionImpl::GetStartTime() const { return startTime; }

uint32_t WsSessionImpl::GetStopTime() const { return stopTime; }

void WsSessionImpl::SetDeadTime(const uint32_t time) { this->deadTime = time; }

uint32_t WsSessionImpl::GetDeadTime() const { return this->deadTime; }

void WsSessionImpl::OnRequestMessage(const std::string &data) {
    if (msgBuffer == nullptr) {
        return;
    }
    if (data.empty()) {
        return;
    }
    (*msgBuffer.get()) << data;
}

void WsSessionImpl::OnResponse(std::unique_ptr<Protocol::Response> responsePtr) {
    if (responsePtr != nullptr) {
        if (useResponseQueue) {
            responseQueue << std::move(responsePtr);
            responseQueueCv.notify_one();
        } else {
            SendResponse(*responsePtr.get());
        }
    }
}

void WsSessionImpl::SendBaseResponse(std::unique_ptr<Protocol::Response> responsePtr) {
    if (responsePtr != nullptr) {
        document_t json(kObjectType);
        ProtocolUtil::SetResponseJsonBaseInfo(*responsePtr.get(), json);
        std::string responseStr = JsonUtil::JsonDump(json);
        responseStr = StringUtil::ToUtf8Str(responseStr);
        SendResponse(*responsePtr.get(), responseStr);
    }
}

void WsSessionImpl::OnEvent(std::unique_ptr<Protocol::Event> eventPtr) {
    if (eventPtr != nullptr) {
        SendEvent(*eventPtr.get());
    }
}

bool WsSessionImpl::Send(const std::string &message) {
    if (GetStatus() == Status::CLOSED) {
        ServerLog::Info("Session has been closed.");
        return false;
    }
    if (channel != nullptr) {
        WsChannel::SendStatus res = channel->send(message, uWS::OpCode::TEXT, false);
        return res == WsChannel::SendStatus::SUCCESS;
    } else {
        ServerLog::Error("Channel is null, so that message cannot be sent.");
        return false;
    }
}

void WsSessionImpl::SendResponse(const Protocol::Response &response) {
    std::string error;
    std::optional<document_t> json = ProtocolManager::Instance().ToJson(response, error);
    if (!json.has_value()) {
        ServerLog::Error(error);
        return;
    }
    std::string responseStr = JsonUtil::JsonDump(json.value());
    responseStr = StringUtil::ToUtf8Str(responseStr);
    SendResponse(response, responseStr);
}

void WsSessionImpl::SendResponse(const Protocol::Response &response, const std::string &responseStr) {
    std::string traceId = TraceIdManager::GetTraceId();
    std::weak_ptr<WsSessionImpl> weakSelf = shared_from_this();
    loop->defer([weakSelf, responseStr, response, traceId]() {
        auto self = weakSelf.lock();
        if (!self) {
            return;
        }
        if (self->GetStatus() == Status::CLOSED) {
            return;
        }
        TraceIdManager::SetTraceId(traceId);
        bool res = self->Send(responseStr);
        ServerLog::Info("Send response status: ", res, ", response result: ", response.result,
            ", command: ", response.command, ", request id = ", response.requestId, ", response id = ", response.id);
    });
}

void WsSessionImpl::SendEvent(Protocol::Event &event) {
    std::string error;
    std::optional<document_t> json = ProtocolManager::Instance().ToJson(event, error);
    if (!json.has_value()) {
        ServerLog::Info(error);
        return;
    }
    std::string eventStr = JsonUtil::JsonDump(json.value());
    eventStr = StringUtil::ToUtf8Str(eventStr);
    std::string traceId = TraceIdManager::GetTraceId();
    std::weak_ptr<WsSessionImpl> weakSelf = shared_from_this();
    loop->defer([weakSelf, eventStr, event, traceId]() {
        auto self = weakSelf.lock();
        if (!self) {
            return;
        }
        if (self->GetStatus() == Status::CLOSED) {
            return;
        }
        TraceIdManager::SetTraceId(traceId);
        bool res = self->Send(eventStr);
        ServerLog::Info("Send event status: ", res, ", event result: ", event.result, ", event name:", event.event,
            ", event id = ", event.id);
    });
}

void WsSessionImpl::Start() {
    SetStatus(Status::STARTED);
    startTime = TimeUtil::Instance().NowUTC();
    if (useResponseQueue) {
        // start response queue handle thread
        onHandleResponseThread = std::make_unique<std::thread>(OnHandleResponseQueue, std::ref(*this));
        onHandleResponseThread->detach();
    }
    // start request message handle thread
    onHandleMsgThread = std::make_unique<std::thread>(OnHandleMsgBuffer, std::ref(*this));
    onHandleMsgThread->detach();
}

void WsSessionImpl::Stop() {
    status = Status::CLOSED;
    if (channel != nullptr) {
        channel->close();
        channel = nullptr;
    }
    stopTime = TimeUtil::Instance().NowUTC();
}

std::string WsSessionImpl::GetDeviceKey() const { return deviceKey; }

void WsSessionImpl::SetDeviceKey(const std::string &device) { deviceKey = device; }

std::string WsSessionImpl::GetBundleName() const { return bundleName; }

void WsSessionImpl::SetBundleName(const std::string &bundle) { bundleName = bundle; }
} // end of namespace Server
} // end of namespace Dic
