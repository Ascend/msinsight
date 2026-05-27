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

#ifndef PROFILER_SERVER_PARSERIE_H
#define PROFILER_SERVER_PARSERIE_H
#include "ServitizationOpenApi.h"
#include "ProjectParserFactory.h"
#include "FileParser.h"
namespace Dic::Module {
class ParserIE : public ProjectParserBase {
  public:
    ParserIE() = default;
    ~ParserIE() override;

    void Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos, ImportActionRequest &request,
        ImportActionResponse &response) final;
    ProjectTypeEnum GetProjectType(const std::string &dataPath) final;
    std::vector<std::string> GetParseFileByImportFile(const std::string &importFile, std::string &error) override;
    static void BuildProjectExploreInfo(ProjectExplorerInfo &projectInfo, const std::vector<std::string> &parsedFiles);
    bool ExistIEFile(const std::string &file);

  protected:
    std::shared_ptr<IE::ServitizationOpenApi> servitizationOpenApi = std::make_shared<IE::ServitizationOpenApi>();

  private:
    std::vector<std::string> FindIEFile(const std::string &path);
    void SetParseCallBack(FileParser &fileParser);
    std::unordered_map<std::string, std::string> GetRankListMap(
        const std::vector<Global::ProjectExplorerInfo> &projectInfos);
    static void ParserTraceData(const std::unordered_map<std::string, std::string> &rankListMap);
    static void FillBaseResponseInfo(const ImportActionRequest &request, ImportActionResponse &response,
        const std::vector<ProjectExplorerInfo> &projectInfos);
};
} // namespace Dic::Module

#endif // PROFILER_SERVER_PARSERIE_H
