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
#include "WsSessionManager.h"
#include "WsDefs.h"
#include "PluginsManager.h"
#include "ParamsParser.h"
#include "ProjectExplorerManager.h"
#include "WsServer.h"

namespace Dic {
namespace Server {
WsServer::WsServer(const std::string &host, int port, bool strictMode)
    : BaseServer(host, port), strictMode(strictMode) {}

bool WsServer::Start() {
    FileUtil::SetStrictMode(strictMode);
    listenStart = false;
    Dic::Core::PluginsManager::LoadPlugins();
    PreLoadEventDir();
    listenThreadPtr = std::make_unique<std::thread>(WsServer::ListenThreadFunc, std::ref(*this));
    if (listenThreadPtr == nullptr) {
        return false;
    }
    listenThreadPtr->detach();
    std::unique_lock<std::mutex> lock(resultMutex);
    resultCv.wait_for(lock, std::chrono::milliseconds(resultTimeout), [&] { return this->listenStart; });
    return listenStart;
}

bool WsServer::Stop() {
    listenStart = false;
    if (wsApp) {
        wsApp->close();
        if (listenThreadPtr->joinable()) {
            listenThreadPtr->join();
        }
    }
    return true;
}

bool WsServer::IsStart() const { return listenStart; }

void WsServer::ListenThreadFunc(WsServer &server) { server.StartListen(); }

void WsServer::StartListen() {
    ServerLog::Info("Start server listen");
    wsApp = std::make_unique<uWS::App>();
    uWS::App::WebSocketBehavior<WsUserData> behavior = CreateWsBehavior();
    wsApp->ws<WsUserData>("/*", std::move(behavior));
    LoadHandlers();
    wsApp->listen(host, port, std::bind(&WsServer::ListenCb, this, std::placeholders::_1));
    ServerLog::Info("Run server");
    wsApp->run();
}

uWS::App::WebSocketBehavior<WsUserData> WsServer::CreateWsBehavior() {
    const int maxPayLoadSize = 16 * 1024 * 1024;
    const int idleTimeout = 120;
    const int maxBackPressureSize = 100 * 1024 * 1024;
    uWS::App::WebSocketBehavior<WsUserData> wsBehavior = {.compression = uWS::SHARED_COMPRESSOR,
        .maxPayloadLength = maxPayLoadSize,
        .idleTimeout = idleTimeout,
        .maxBackpressure = maxBackPressureSize,
        .closeOnBackpressureLimit = false,
        .resetIdleTimeoutOnSend = false,
        .sendPingsAutomatically = true,
        .maxLifetime = 0,
        .upgrade =
            [](auto *res, uWS::HttpRequest *req, auto *context) {
                std::string_view url = req->getUrl();
                res->template upgrade<WsUserData>({.reqUrl = url.data()}, req->getHeader("sec-websocket-key"),
                    req->getHeader("sec-websocket-protocol"), req->getHeader("sec-websocket-extensions"), context);
            },
        .open = std::bind(&WsServer::OnOpenCb, this, std::placeholders::_1),
        .message = std::bind(
            &WsServer::OnMessageCb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        .drain = nullptr,
        .ping = nullptr,
        .pong = nullptr,
        .subscription = nullptr,
        .close =
            std::bind(&WsServer::OnCloseCb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)};
    return wsBehavior;
}

void WsServer::ListenCb(us_listen_socket_t *listenSocket) {
    if (listenSocket == nullptr) {
        ServerLog::Error("Failed to start server, listen error.");
        return;
    }
    listenStart = true;
    resultCv.notify_one();
    ServerLog::Info("Start server successfully.");
}

void WsServer::OnOpenCb(WsChannel *ws) {
    ServerLog::Info("Accept new session, channel = ", ws);
    if (ws == nullptr) {
        ServerLog::Info("Accept new session, channel is null ");
        return;
    }
    std::string url = ws->getUserData()->reqUrl;
    if (url.empty()) {
        ServerLog::Info("Accept new session failed, channel = ", ws, " url is null");
        ws->end(URL_NULL_CODE, "url is null");
        return;
    }

    if (WsSessionManager::Instance().GetSession() != nullptr) {
        ws->end(REDUNDANT_CONNECTION_CODE, WS_CLOSE_CODE_REASON.at(REDUNDANT_CONNECTION_CODE));
        ServerLog::Error("Not Connect, already connecting");
        return;
    }
    auto session = std::make_shared<WsSessionImpl>(ws);
    WsSessionManager::Instance().AddSession(session);
}

void WsServer::OnCloseCb(WsChannel *ws, int code, std::string_view message) {
    ServerLog::Info("Session close, channel = ", ws, ", code = ", code, ", message = ", message);
    if (ws == nullptr) {
        return;
    }
    WsSessionImpl *session = dynamic_cast<WsSessionImpl *>(WsSessionManager::Instance().GetSession());
    if (session != nullptr && ws == session->GetChannel()) {
        session->SetStatus(WsSessionImpl::Status::CLOSED);
        session->WaitForExit();
        WsSessionManager::Instance().RemoveSession();
        ServerLog::Info("Session remove ok.");
    }
}

void WsServer::OnMessageCb(WsChannel *ws, std::string_view message, uWS::OpCode opCode) {
    if (ws == nullptr) {
        return;
    }
    WsSessionImpl *session = dynamic_cast<WsSessionImpl *>(WsSessionManager::Instance().GetSession());
    if (session == nullptr) {
        ServerLog::Error("Session is not valid, it will be closed at server.");
        return;
    }
    session->OnRequestMessage(std::string(message));
}

void WsServer::LoadHandlers() {
    auto &pluginManager = Dic::Core::PluginsManager::Instance();
    for (const auto &[key, plugin] : pluginManager.GetAllPlugins()) {
        for (const auto &[prefix, handler] : plugin->GetAllHandlers()) {
            if (handler->GetApiType() == Core::API_TYPE::GET) {
                AddGetHandler("/" + key + "/" + prefix, handler);
            } else {
                AddPostHandler("/" + key + "/" + prefix, handler);
            }
        }
    }
}

void WsServer::AddGetHandler(const std::string &key, std::shared_ptr<Core::ApiHandler> handler) {
    wsApp->get(key.data(), [handler](uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
        // add coc
        res->writeHeader("Access-Control-Allow-Origin", "*");
        res->writeHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res->writeHeader("Access-Control-Allow-Headers", "Content-Type");
        std::string result;
        handler->run(req->getQuery(), result);
        res->end(result);
    });
}

void WsServer::AddPostHandler(const std::string &key, std::shared_ptr<Core::ApiHandler> handler) {
    wsApp->post(key.data(), [handler](uWS::HttpResponse<false> *res, auto *req) {
        res->onAborted([]() { uWS::Loop::get()->defer([]() {}); });
        res->onData([res, handler, bodyBuffer = std::string()](std::string_view data, bool isEnd) mutable {
            bodyBuffer.append(data);
            if (isEnd) {
                // add coc
                res->writeHeader("Access-Control-Allow-Origin", "*");
                res->writeHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                res->writeHeader("Access-Control-Allow-Headers", "Content-Type");
                // 处理数据
                std::string result;
                handler->run(bodyBuffer, result);
                res->end(result);
            }
        });
    });
}

void WsServer::PreLoadEventDir() {
    const auto &opt = ParamsParser::Instance().GetOption();
    std::string eventDir = opt.eventDir;
    if (eventDir.empty()) {
        return;
    }
    if (!FileUtil::IsFolder(eventDir) || !FileUtil::CheckPathSecurity(eventDir, CHECK_FILE_READ)) {
        return;
    }
    std::vector<Dic::Module::Global::ProjectExplorerInfo> projectExplorerInfos;
    Dic::Module::Global::ProjectExplorerInfo projectExplorerInfo;
    projectExplorerInfo.fileName = eventDir;
    projectExplorerInfo.projectName = eventDir;
    projectExplorerInfo.projectType = 0;
    projectExplorerInfo.importType = "import";
    auto fileInfo = std::make_shared<Dic::Module::Global::ParseFileInfo>();
    fileInfo->fileId = "";
    fileInfo->parseFilePath = eventDir;
    projectExplorerInfo.subParseFileInfo.emplace_back(fileInfo);
    Dic::Module::Global::ProjectExplorerManager::Instance().SaveProjectExplorer(projectExplorerInfo, false);
}
} // end of namespace Server
} // end of namespace Dic
