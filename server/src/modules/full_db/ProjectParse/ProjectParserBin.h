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

#ifndef PROFILER_SERVER_PARSERBIN_H
#define PROFILER_SERVER_PARSERBIN_H

#include "ProjectParserFactory.h"
#include "TimelineRequestHandler.h"
#include "FileParser.h"
#include "TraceFileParser.h"

namespace Dic::Module {
using namespace Dic::Module::Global;
class ProjectParserBin : public ProjectParserBase {
  public:
    ProjectParserBin() = default;
    ~ProjectParserBin() override = default;

    void Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos, ImportActionRequest &request,
        ImportActionResponse &response) final;
    void ParserBaseline(const Global::ProjectExplorerInfo &projectInfo, Global::BaselineInfo &baselineInfo) final;
    ProjectTypeEnum GetProjectType(const std::string &dataPath) final;

    std::vector<std::string> GetParseFileByImportFile(const std::string &importFile, std::string &error) override {
        return {importFile};
    }
    static void BuildProjectExploreInfo(ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles);
    static void BuildProjectInfoFromParseFile(ProjectExplorerInfo &projectInfo, const std::string &parsedFile);

  private:
    void HandleCompute(ImportActionResponse &response, const std::string &selectedFolder);
    std::vector<std::pair<std::string, std::string>> GetSimulationTraceFiles(
        const std::string &selectFilePath, ImportActionResBody &body);
    static void SetParseCallBack(FileParser &fileParser);
};
} // end of namespace Dic::Module

#endif // PROFILER_SERVER_PARSERBIN_H
