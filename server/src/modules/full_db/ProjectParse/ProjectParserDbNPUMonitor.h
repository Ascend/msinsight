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

#ifndef PROFILER_SERVER_PARSERDBNPUMONITOR_H
#define PROFILER_SERVER_PARSERDBNPUMONITOR_H

#include "ProjectParserDb.h"

namespace Dic {
namespace Module {
class ProjectParserDbNPUMonitor : public ProjectParserDb {
  public:
    ProjectParserDbNPUMonitor() = default;
    ~ProjectParserDbNPUMonitor() override = default;

    ProjectTypeEnum GetProjectType(const std::string &dataPath) final;
    std::vector<std::string> GetParseFileByImportFile(const std::string &importFile, std::string &error) final;
    static void BuildProjectExploreInfo(ProjectExplorerInfo &info, const std::vector<std::string> &parsedFiles);
    void ParserBaseline(const Global::ProjectExplorerInfo &projectInfo, Global::BaselineInfo &baselineInfo) final;
};
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_PARSERDBNPUMONITOR_H
