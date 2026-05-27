/*
* -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#ifndef PROFILER_SERVER_PROJECTPARSERSNAPSHOT_H
#define PROFILER_SERVER_PROJECTPARSERSNAPSHOT_H
#include "ProjectParserFactory.h"

namespace Dic::Module {
class ProjectParserMemSnapshot : public ProjectParserBase {
  public:
    ProjectParserMemSnapshot() = default;
    ~ProjectParserMemSnapshot() override = default;
    void Parser(const std::vector<ProjectExplorerInfo> &projectInfos, ImportActionRequest &request,
        ImportActionResponse &response) override;
    ProjectTypeEnum GetProjectType(const std::string &dataPath) override;
    std::vector<std::string> GetParseFileByImportFile(const std::string &importFile, std::string &error) override;
    // 覆盖重写
    static void BuildProjectExploreInfo(ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles);
    static bool IsSnapshotPickleFile(const std::string &filename);
};
} // namespace Dic::Module
#endif //PROFILER_SERVER_PROJECTPARSERSNAPSHOT_H
