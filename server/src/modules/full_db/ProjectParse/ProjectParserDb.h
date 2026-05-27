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

#ifndef PROFILER_SERVER_PARSERDB_H
#define PROFILER_SERVER_PARSERDB_H

#include "ProjectParserFactory.h"
#include "TimelineRequestHandler.h"
#include "DbTraceDataBase.h"

namespace Dic {
namespace Module {
using HostInfo = std::map<std::string, std::vector<std::string>>;

class ProjectParserDb : public ProjectParserBase {
  public:
    ProjectParserDb() = default;
    ~ProjectParserDb() override = default;

    void Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos, ImportActionRequest &request,
        ImportActionResponse &response) final;
    void ParserBaseline(const Global::ProjectExplorerInfo &projectInfo, Global::BaselineInfo &baselineInfo) override;
    ProjectTypeEnum GetProjectType(const std::string &dataPath) override;
    std::vector<std::string> GetParseFileByImportFile(const std::string &importFile, std::string &error) override;
    static void BuildProjectExploreInfo(ProjectExplorerInfo &info, const std::vector<std::string> &parsedFiles);
    static void BuildProjectFromParseFile(ProjectExplorerInfo &info, const std::string &parsedFile);
    static std::string GetFileIdWithDb(const std::string &filePath);

  protected:
    std::map<std::string, HostInfo> GetReportFiles(const std::vector<ProjectExplorerInfo> &projectInfos,
        std::optional<std::string> parseFilePathFilter = std::nullopt);
    void GetReportFilesOneFile(const Dic::Module::Global::ProjectExplorerInfo &project,
        std::map<std::string, HostInfo> &hostMap, std::shared_ptr<ParseFileInfo> parsefileInfo,
        const std::string &file);

    void SetRankDeviceMap(std::shared_ptr<ParseFileInfo> parseFileInfo,
        std::unordered_map<std::string, std::string> &rankDeviceMap, const std::string &deviceIdInMem,
        const std::string &rank);
    static std::vector<std::string> GetDbFilesInDir(const std::string &filePath);
    void SetParseCallBack();
    static void SetBaseActionOfResponse(ImportActionResponse &response, const std::string &rankId,
        const std::string &host, const std::string &dbFile, int64_t projectType);
    static void SetHostInfo(
        std::map<std::string, HostInfo> &hostInfoMap, ImportActionResponse &response, int64_t projectType);

    static void ClusterProcess(std::shared_ptr<ParseFileInfo> clusterInfo, bool isCluster,
        ProjectTypeEnum curProjectTypeEnum, std::map<std::string, std::vector<std::string>> &dataPathToDbMap,
        const std::string &projectName);
    static void ParseBaselineClusterInfo(const Global::ProjectExplorerInfo &projectInfos, BaselineInfo &baselineInfo);
    void ParseClusterInfo(
        const std::vector<Global::ProjectExplorerInfo> &projectInfos, bool isCluster, ProjectTypeEnum projectType);
    static void FilterHostMap(std::map<std::string, HostInfo> &hostInfoMap, const std::string &filePath);

    std::shared_ptr<FullDb::DbTraceDataBase> GetTraceDbConnect(const std::string &fileId);

    std::string GetBaselineDbFile(const std::string &path);
};
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_PARSERDB_H
