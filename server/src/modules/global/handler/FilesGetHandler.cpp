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
#include "FileSelector.h"
#include "WsSessionManager.h"
#include "FilesGetHandler.h"

namespace Dic {
namespace Module {
namespace Global {
using namespace Dic::Server;
bool FilesGetHandler::HandleRequest(std::unique_ptr<Request> requestPtr) {
    FilesGetRequest &request = dynamic_cast<FilesGetRequest &>(*requestPtr.get());
    ServerLog::Info("Files get start");
    std::unique_ptr<FilesGetResponse> responsePtr = std::make_unique<FilesGetResponse>();
    FilesGetResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    response.body.path = request.params.path;
    FileSelector::GetFoldersAndFiles(
        request.params.path, response.body.childrenFolders, response.body.childrenFiles, response.body.exist);
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Global
} // Module
} // Dic
