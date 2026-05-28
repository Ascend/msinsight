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
#include "WsSessionManager.h"
#include "ProjectExplorerManager.h"
#include "GetProjectExplorerInfoHandler.h"

namespace Dic {
namespace Module {
using namespace Dic::Server;
using namespace Global;

bool GetProjectExplorerInfoHandler::HandleRequest(std::unique_ptr<Request> requestPtr) {
    auto &request = dynamic_cast<ProjectExplorerInfoGetRequest &>(*requestPtr.get());
    std::unique_ptr<ProjectExplorerInfoGetResponse> responsePtr = std::make_unique<ProjectExplorerInfoGetResponse>();
    ProjectExplorerInfoGetResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    std::vector<ProjectExplorerInfo> infos =
        ProjectExplorerManager::Instance().QueryProjectExplorer("", std::vector<std::string>());
    // adapt one project have two import path, merge by name
    std::map<std::string, std::vector<std::string>> res;
    std::map<std::string, ProjectExplorerInfo> infoMap;
    for (const auto &info : infos) {
        if (infoMap.find(info.projectName) != infoMap.end()) {
            infoMap[info.projectName].MergeProjectExploreInfo(info);
            continue;
        }
        infoMap[info.projectName] = info;
    }
    for (const auto &[name, info] : infoMap) {
        Protocol::ProjectDirectoryInfo temp;
        temp.projectName = name;
        temp.fileName = info.subParseFileInfo;
        temp.projectTree = info.projectFileTree;
        res.erase(info.projectName);
        response.body.projectDirectoryList.push_back(temp);
    }

    SendResponse(std::move(responsePtr), true);
    return true;
}
} // end of namespace Module
} // Dic
