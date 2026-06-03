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

#ifndef PROFILER_SERVER_PARSERJSON_H
#define PROFILER_SERVER_PARSERJSON_H

#include "ProjectParserFactory.h"
#include "TimelineRequestHandler.h"
#include "ClusterFileParser.h"
#include "FileParser.h"
#include "FileReader.h"
#include "TraceFileParser.h"

namespace Dic {
namespace Module {
class ProjectParserJson : public ProjectParserBase {
  public:
    explicit ProjectParserJson(Timeline::TraceFileParser &parser) : _fileParser(parser) {
        fileReader = std::make_unique<FileReader>();
    }

    ~ProjectParserJson() override = default;

    void Parser(const std::vector<Global::ProjectExplorerInfo> &projectInfos, ImportActionRequest &request,
        ImportActionResponse &response) final;

    void ParserBaseline(const Global::ProjectExplorerInfo &projectInfo, Global::BaselineInfo &baselineInfo) final;

    ProjectTypeEnum GetProjectType(const std::string &dataPath) final;

    std::vector<std::string> GetParseFileByImportFile(const std::string &importFile, std::string &error) final;

    static bool ExistJsonFormatFile(const std::string &file);

    /**
     * 判断指定文件是否为 ACLGraphDebugJSON 格式
     * @param filePath 文件路径
     * @return 若前三行中存在 "pid": "*aclGraph"（区分大小写）则返回 true
     */
    static bool IsACLGraphDebugJSON(const std::string &filePath);

    static bool IsFtraceJsonData(const std::string &filePath);

    static void BuildProjectExploreInfo(ProjectExplorerInfo &info, const std::vector<std::string> &parsedFiles);

    static void BuildProjectFromParseFile(ProjectExplorerInfo &info, const std::string &parseFile);

    static std::string GetSubId(const std::string &filePath, ParseFileType type);

    static std::string GetDeviceId(const std::string &parseFolder, const std::string &rankId);

    static std::string GetDeviceIdFromMemory(const std::string &parseFolder);

    static std::string GetDeviceIdFromKernel(const std::string &parseFolder);

    static std::string GetDeviceIdFromCSVFile(const std::string &filePath);

    static std::string GetDeviceIdFromPath(const std::string &parseFolder);

  protected:
    bool CheckParseFileInfoSize(
        const std::shared_ptr<Global::ParseFileInfo> &parseFileInfo, std::vector<std::string> &jsonFiles) const;

    static std::tuple<bool, bool, bool> CheckHasJsonMemoryDataOperatorData(
        const std::vector<Global::ProjectExplorerInfo> &projectInfos);

    static std::string GetFileIdWithDb(const std::string &filePath);

  private:
    Timeline::TraceFileParser &_fileParser;

    std::vector<std::string> FindAllTraceFile(const std::string &path, std::string &error);

    static std::vector<std::string> FindTraceFile(const std::string &path, std::string &error, std::string &curScene);

    static void FindTraceFiles(const std::string &path, int depth, std::string &error,
        std::vector<std::string> &traceFiles, std::string &curScene);

    static void FindAscendFolder(const std::string &path, std::vector<std::string> &traceFiles);

    static bool IsJsonValid(const std::string &fileName);

    static void ClusterProcess(std::shared_ptr<ParseFileInfo> clusterInfo, ProjectTypeEnum projectType,
        bool isShowCluster, std::map<std::string, std::vector<std::string>> &dataPathToDbMap,
        const std::string &projectName);

    static void ClusterProcessAsyncStep(Timeline::ClusterFileParser clusterFileParser);

    void SetParseCallBack(FileParser &fileParser);

    bool isSimulation(std::string filePath);

    std::map<std::string, RankEntry> GetRankEntryMap(
        const std::vector<Global::ProjectExplorerInfo> &projectInfos, bool isBaseline);

    std::string AddSuffixToDuplicatedRankId(
        const std::map<std::string, RankEntry> &rankToTraceMap, const std::string &rankId);

    std::vector<std::string> GetJsonFileUnderFolder(const std::string &path);

    void ParserJsonData(const std::map<std::string, RankEntry> &rankListMap,
        const std::vector<Global::ProjectExplorerInfo> &projectInfos, bool isShowCluster);

    static void FillBaseResponseInfo(const ImportActionRequest &request, ImportActionResponse &response,
        const std::vector<Global::ProjectExplorerInfo> &projectInfos);

    static void ParserClusterBaseline(const Global::ProjectExplorerInfo &projectInfo, BaselineInfo &baselineInfo);

    void ParserSingleCardBaseline(const Global::ProjectExplorerInfo &projectInfos, Global::BaselineInfo &baselineInfo);

    static void ParserMetaData(const std::vector<Global::ProjectExplorerInfo> &projectInfos);

    static void UpdateRankIdToDevice(std::map<std::string, RankEntry> &rankEntry);

    void SetBaseAction(
        const std::map<std::string, RankEntry> &rankListMap, ImportActionResponse &response, int64_t projectType);

    void ParseBaselineTraceFile(const std::vector<std::string> &jsonFiles, const std::string &rankId,
        const std::string &fileId, const std::string &filePath);
};
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_PARSERJSON_H
