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

#ifndef PROFILER_SERVER_SYSTEMMEMORYDATABASE_H
#define PROFILER_SERVER_SYSTEMMEMORYDATABASE_H

#include "Database.h"
#include "SystemMemoryDatabaseDef.h"
#include "vector"

namespace Dic {
namespace Module {
namespace Global {
class SystemMemoryDatabase : public Database {
  public:
    explicit SystemMemoryDatabase(std::recursive_mutex &sqlMutex) : Database(sqlMutex) {};
    ~SystemMemoryDatabase() override = default;

    bool SetConfig() override;
    bool CreateTable();
    std::vector<ProjectExplorerInfo> QueryProjectExplorerData(
        const std::vector<std::string> &projectNameList, const std::vector<std::string> &fileNameList);
    bool InsertDuplicateUpdateProject(const ProjectExplorerInfo &projectExplorerInfo);
    bool InsertDuplicateUpdateParsedFile(const std::vector<std::shared_ptr<ParseFileInfo>> &parseFileInfoList);
    bool UpdateProjectName(const std::string &oldProjectName, const std::string &newProjectName);
    bool UpdateProjectDbPath(const std::string &projectName, const std::string &fileName, const std::string &dbPath);
    bool DropTable();
    bool DeleteFileMenu(const std::vector<std::string> &projectNameList, const std::vector<int64_t> &projectIdList);
    bool DeleteParsedFile(const std::vector<int64_t> &projectIdList, const std::vector<int64_t> &idList);

    std::map<int64_t, std::vector<std::shared_ptr<ParseFileInfo>>> QueryParseFileInfo(
        const std::vector<int64_t> &projectExplorerIdList, const std::vector<std::string> &parsePathList);

  private:
    const std::string projectExplorerTable = "project_explorer";
    const std::string parseFileInfoTable = "parse_file_info";
};
}
}
}

#endif // PROFILER_SERVER_SYSTEMMEMORYDATABASE_H
