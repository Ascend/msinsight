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

#ifndef PROFILER_SERVER_FILEMENUMANAGER_H
#define PROFILER_SERVER_FILEMENUMANAGER_H

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include "GlobalProtocolResponse.h"
#include "SystemMemoryDatabase.h"

namespace Dic {
namespace Module {
namespace Global {
class ProjectExplorerManager {
  public:
    static ProjectExplorerManager &Instance();
    ProjectExplorerManager() = default;
    ~ProjectExplorerManager() = default;
    bool UpdateProjectName(const std::string &oldProjectName, const std::string &newProjectName);
    std::vector<ProjectExplorerInfo> QueryProjectExplorer(
        const std::string &projectName, const std::vector<std::string> &dataPathList);
    bool SaveProjectExplorer(const ProjectExplorerInfo &projectExplorerInfo, bool isConflict);
    bool DeleteProjectAndFilePath(const std::string &projectName, const std::vector<std::string> &filePathList);
    Dic::Protocol::ProjectErrorType CheckProjectConflict(const std::string &projectName, const std::string &filePath);
    void UpdateProjectDbPath(
        const std::string &projectName, const std::map<std::string, std::vector<std::string>> &dataPathToDbMap);
    void InitSystemMemoryDbPath(const std::string &filePath);
    bool IsClusterData(const std::string &projectName);
    bool ClearProjectExplorer(const std::vector<std::string> &projectNameList);
    static ProjectTypeEnum GetProjectType(const std::vector<ProjectExplorerInfo> &projectInfo);
    static std::vector<std::shared_ptr<ParseFileInfo>> GetClusterFilePath(
        const std::vector<ProjectExplorerInfo> &projectInfo);

    bool IsTextMultiCluster(const std::string &projectName);

    bool UpdateParseFileInfo(
        const std::string &projectName, const std::vector<std::shared_ptr<ParseFileInfo>> &parseFileInfo);

  private:
    std::string systemMemoryDbPath;
    std::recursive_mutex mutex;
    std::unique_ptr<Global::SystemMemoryDatabase> db;

    bool InitSystemMemoryDb();
    bool SaveProjectExplorerToDb(const std::string &projectName, const ProjectExplorerInfo &projectExplorerInfo);
    static void RebuildParseFileInfo(
        ProjectExplorerInfo &projectInfo, std::vector<std::shared_ptr<ParseFileInfo>> &parseFileInfos);
    std::string eventDir;
};
}
}
}
#endif // PROFILER_SERVER_FILEMENUMANAGER_H
