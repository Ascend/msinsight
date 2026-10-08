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
#include <algorithm>
#include "ProjectExplorerManager.h"
#include "ProjectParserFactory.h"
#include "BaselineManager.h"
#include "DataBaseManager.h"
#include "SourceFileParser.h"
#include "ParallelStrategyAlgorithmManager.h"
#include "ParserStatusManager.h"
#include "BaselineManagerService.h"
using namespace Dic::Module::Summary;
namespace Dic {
namespace Module {
namespace Global {
namespace {
int64_t GetCompareDataType(int64_t projectType) {
    if (projectType == static_cast<int64_t>(ProjectTypeEnum::DB_CLUSTER)) {
        return static_cast<int64_t>(ProjectTypeEnum::DB);
    }
    if (projectType == static_cast<int64_t>(ProjectTypeEnum::TEXT_CLUSTER)) {
        return static_cast<int64_t>(ProjectTypeEnum::TRACE);
    }
    return projectType;
}

bool HasSingleCompareDataType(const std::vector<ProjectExplorerInfo> &projects) {
    const auto projectType = GetCompareDataType(projects.front().projectType);
    return std::all_of(projects.begin(), projects.end(),
        [projectType](const auto &project) { return GetCompareDataType(project.projectType) == projectType; });
}

bool HasMemScopeData(const std::vector<ProjectExplorerInfo> &projects) {
    return std::any_of(projects.begin(), projects.end(), [](const auto &project) {
        return std::any_of(project.subParseFileInfo.begin(), project.subParseFileInfo.end(), [](const auto &fileInfo) {
            return fileInfo &&
                std::regex_match(FileUtil::GetFileName(fileInfo->parseFilePath), std::regex(memScopeDbReg));
        });
    });
}
}

void BaselineManagerService::ResetBaseline(bool force) {
    // 没有解析进程时才可以reset
    auto baselineId = Dic::Module::Timeline::BaselineManager::Instance().GetBaselineId();
    Dic::Module::Timeline::ParserStatusManager::Instance().WaitAllFinished({baselineId});
    Dic::Module::Timeline::DataBaseManager::Instance().ResetBaseline(force);
    BaselineManager::Instance().Reset();
    Source::SourceFileParser::Instance().ResetBaseline();
}

bool BaselineManagerService::CheckIsSupportCompare(const std::vector<ProjectExplorerInfo> &baseline,
    const std::vector<ProjectExplorerInfo> &cur, std::string &errorMsg, const std::string &filePath) {
    if (baseline.empty() || cur.empty()) {
        return false;
    }

    // 同类型数据追加导入会产生多条记录，记录数不代表数据类型数。
    if (!HasSingleCompareDataType(baseline) || !HasSingleCompareDataType(cur)) {
        errorMsg = "Multi data type sense not support compare yet.";
        return false;
    }
    // 多device场景不允许设置基线
    const auto deviceCount = std::count_if(baseline[0].subParseFileInfo.begin(), baseline[0].subParseFileInfo.end(),
        [&filePath](const auto &fileInfo) { return fileInfo && fileInfo->parseFilePath == filePath; });
    if (deviceCount > 1) {
        errorMsg = "Multi device scenario does not support setting comparison.";
        return false;
    }
    // MemScope不支持对比
    if (HasMemScopeData(baseline) || HasMemScopeData(cur)) {
        errorMsg = "MemScope data does not support comparison function.";
        return false;
    }

    // 只有部分数据类型支持设置对比，如果非预期数据类型，则直接给前端返回错误提示
    auto projectTypeEnum = static_cast<ProjectTypeEnum>(baseline[0].projectType);
    if (!IsSupportCompareType(projectTypeEnum)) {
        errorMsg = "Not supported to set the project type for baseline!";
        return false;
    }

    auto curProjectTypeEnum = static_cast<ProjectTypeEnum>(cur[0].projectType);
    if (!IsSupportCompareType(curProjectTypeEnum)) {
        errorMsg = "The current project type does not support comparison function.";
        return false;
    }
    if (!IsComparable(projectTypeEnum, curProjectTypeEnum)) {
        errorMsg = "Comparison is not allowed for different data types.";
        return false;
    }
    return true;
}

bool BaselineManagerService::InitBaselineData(
    const Protocol::BaselineSettingRequest &request, BaselineInfo &baselineInfo) {
    ResetBaseline(true);
    // 多集群场景下需要把所有的信息查出来
    std::vector<ProjectExplorerInfo> projectExplorerList =
        ProjectExplorerManager::Instance().QueryProjectExplorer(request.params.projectName, {});
    std::vector<ProjectExplorerInfo> curProject =
        ProjectExplorerManager::Instance().QueryProjectExplorer(request.projectName, {});
    if (projectExplorerList.empty() || curProject.empty()) {
        baselineInfo.errorMessage = "The project does not exist, baseline setting failed.";
        return false;
    }

    const auto matchesBaselinePath = [&request](const ProjectExplorerInfo &project) {
        const auto &filePath = request.params.filePath;
        return !filePath.empty() &&
            (project.fileName == filePath ||
                std::any_of(project.fileInfoMap.begin(), project.fileInfoMap.end(), [&filePath](const auto &entry) {
                    return entry.second && entry.second->parseFilePath == filePath;
                }));
    };
    auto baselineProject = std::find_if(projectExplorerList.begin(), projectExplorerList.end(), matchesBaselinePath);
    if (baselineProject == projectExplorerList.end()) {
        baselineInfo.errorMessage = "The baseline path does not exist in the project.";
        return true;
    }
    if (std::find_if(baselineProject + 1, projectExplorerList.end(), matchesBaselinePath) !=
        projectExplorerList.end()) {
        baselineInfo.errorMessage = "The baseline path belongs to multiple imported records.";
        return true;
    }
    // 将选中卡所属记录用于基线校验，同时保留全部记录检查混合类型。
    std::iter_swap(projectExplorerList.begin(), baselineProject);
    // 检查是否支持对比，返回true是因为目前如果返回false则错误信息前端获取不到，返回false但errorMessage不为空，前端能正确识别到错误并提示
    if (!CheckIsSupportCompare(projectExplorerList, curProject, baselineInfo.errorMessage, request.params.filePath)) {
        return true;
    }
    // 仅解析选中的导入记录，保留其完整目录树以定位集群和通信数据。
    projectExplorerList.resize(1);
    auto projectTypeEnum = ProjectExplorerManager::GetProjectType(projectExplorerList);
    // 根据二级目录判断是否为集群数据
    baselineInfo.isCluster = IsClusterBaseline(projectTypeEnum, projectExplorerList, request.params.filePath);
    if (baselineInfo.isCluster) {
        BaselineManager::Instance().SetBaselineClusterPath(baselineInfo.clusterBaseLine);
    }
    // 获取解析类型（以进一步调用对应解析类）
    ParserType parserType = coverProjectTypeToParserType(projectTypeEnum);
    // 调用工厂进行内容解析
    std::shared_ptr<ProjectParserBase> parser = ParserFactory::GetProjectParser(parserType);
    parser->ParserBaseline(projectExplorerList[0], baselineInfo);
    // 集群场景 初始化并行策略
    if (baselineInfo.isCluster) {
        InitBaselineParallelStrategy(request.params.currentClusterPath);
    }
    return true;
}

bool BaselineManagerService::IsClusterBaseline(ProjectTypeEnum projectTypeEnum,
    const std::vector<ProjectExplorerInfo> &projectInfoList, const std::string &filePath) {
    // 如果非text和db场景，则直接判断为非集群场景，返回false
    if (projectTypeEnum != ProjectTypeEnum::TEXT_CLUSTER && projectTypeEnum != ProjectTypeEnum::DB_CLUSTER) {
        return false;
    }
    bool isCluster = std::any_of(projectInfoList.begin(), projectInfoList.end(), [filePath](const auto &item) {
        auto cluster = item.GetClusterInfos();
        if (cluster.empty()) {
            return false;
        }
        return std::any_of(
            cluster.begin(), cluster.end(), [&filePath](const std::shared_ptr<ParseFileInfo> &clusterInfo) {
                return clusterInfo->parseFilePath == filePath;
            });
    });
    if (isCluster) {
        return true;
    }
    // 整个项目当作集群
    if (!projectInfoList.empty() && !projectInfoList[0].projectFileTree.empty() &&
        projectInfoList[0].fileName == filePath) {
        return true;
    }
    return false;
}

void BaselineManagerService::InitBaselineParallelStrategy(const std::string &compareClusterPath) {
    // 初始化baseline的并行策略，通过同步当前compare数据的并行策略
    auto database = FullDb::DataBaseManager::Instance().GetClusterDatabase(compareClusterPath);
    if (database == nullptr) {
        return;
    }
    auto config = ParallelStrategyAlgorithmManager::Instance().GetParallelStrategyConfig(database->GetDbPath());
    std::string errMsg;
    auto baselineDb =
        FullDb::DataBaseManager::Instance().GetClusterDatabase(BaselineManager::Instance().GetBaseLineClusterPath());
    if (baselineDb == nullptr) {
        return;
    }
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(baselineDb->GetDbPath(), config, errMsg);
}
}
}
}
