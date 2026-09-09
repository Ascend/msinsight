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

#ifndef PROFILER_SERVER_PROJECTPARSERPYTORCHTRACE_H
#define PROFILER_SERVER_PROJECTPARSERPYTORCHTRACE_H

#include "ProjectParserFactory.h"

namespace Dic::Module {

class ProjectParserJson;

class ProjectParserPytorchTrace : public ProjectParserBase {
  public:
    ProjectParserPytorchTrace();
    ~ProjectParserPytorchTrace() override;

    void Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos, ImportActionRequest &request,
        ImportActionResponse &response) final;

    void ParserBaseline(const Global::ProjectExplorerInfo &projectInfo, Global::BaselineInfo &baselineInfo) final;

    ProjectTypeEnum GetProjectType(const std::string &dataPath) final;

    std::vector<std::string> GetParseFileByImportFile(const std::string &importFile, std::string &error) final;

    static bool IsPytorchTraceFile(const std::string &path);

    static bool HasPytorchTraceFile(const std::string &path);

    static std::string GetDirectFileDbPath(const std::string &filePath);

    static void BuildProjectExploreInfo(ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles);

  private:
    std::unique_ptr<ProjectParserJson> jsonParser;
};

} // namespace Dic::Module

#endif // PROFILER_SERVER_PROJECTPARSERPYTORCHTRACE_H
