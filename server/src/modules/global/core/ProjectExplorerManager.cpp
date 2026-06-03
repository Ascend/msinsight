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
#include <unordered_map>
#include <algorithm>
#include "ParamsParser.h"
#include "pch.h"
#include "SystemMemoryDatabase.h"
#include "SystemMemoryDatabaseDef.h"
#include "ProjectParserFactory.h"
#include "TimeUtil.h"
#include "ProjectExplorerManager.h"
#include "ProjectParserJson.h"

namespace Dic::Module::Global {
namespace {
bool IsFtraceProfilingCompatible(ProjectTypeEnum importProjectType, const std::string &importFilePath,
    ProjectTypeEnum existedProjectType, const std::string &existedFilePath) {
    auto isFtraceData = [](ProjectTypeEnum projectType, const std::string &filePath) {
        if (projectType == ProjectTypeEnum::DB_FTRACE) {
            return true;
        }
        if (projectType != ProjectTypeEnum::TRACE && projectType != ProjectTypeEnum::TEXT_CLUSTER) {
            return false;
        }
        return ProjectParserJson::IsFtraceJsonData(filePath);
    };
    auto isProfilingData = [](ProjectTypeEnum projectType) {
        switch (projectType) {
        case ProjectTypeEnum::DB:
        case ProjectTypeEnum::DB_CLUSTER:
        case ProjectTypeEnum::DB_NPUMONITOR:
        case ProjectTypeEnum::TRACE:
        case ProjectTypeEnum::TEXT_CLUSTER:
            return true;
        default:
            return false;
        }
    };

    bool importIsFtrace = isFtraceData(importProjectType, importFilePath);
    bool existedIsFtrace = isFtraceData(existedProjectType, existedFilePath);
    if (!importIsFtrace && !existedIsFtrace) {
        return false;
    }
    if (importIsFtrace && existedIsFtrace) {
        return true;
    }
    return (importIsFtrace && isProfilingData(existedProjectType)) ||
        (existedIsFtrace && isProfilingData(importProjectType));
}
} // namespace

ProjectExplorerManager &ProjectExplorerManager::Instance() {
    static ProjectExplorerManager instance;
    return instance;
}

bool ProjectExplorerManager::UpdateProjectName(const std::string &oldProjectName, const std::string &newProjectName) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!InitSystemMemoryDb()) {
        Server::ServerLog::Error("Failed to open database. path:", systemMemoryDbPath);
        return false;
    }
    if (!db->UpdateProjectName(oldProjectName, newProjectName)) {
        return false;
    }
    return true;
}

std::vector<ProjectExplorerInfo> ProjectExplorerManager::QueryProjectExplorer(
    const std::string &projectName, const std::vector<std::string> &dataPathList) {
    if (!InitSystemMemoryDb()) {
        Server::ServerLog::Error("Failed to open database. path:", systemMemoryDbPath);
        return {};
    }
    std::vector<std::string> projectNameList;
    if (!projectName.empty()) {
        projectNameList.push_back(projectName);
    }
    std::vector<ProjectExplorerInfo> projectExplorerList =
        db->QueryProjectExplorerData(projectNameList, std::vector<std::string>());
    std::vector<int64_t> projectExplorerIdList;
    for (const auto &item : projectExplorerList) {
        projectExplorerIdList.push_back(item.id);
    }
    std::map<int64_t, std::vector<std::shared_ptr<ParseFileInfo>>> parseFileInfoMap =
        db->QueryParseFileInfo(projectExplorerIdList, dataPathList);
    std::vector<ProjectExplorerInfo> res;
    for (auto &item : projectExplorerList) {
        if (parseFileInfoMap.find(item.id) != parseFileInfoMap.end()) {
            RebuildParseFileInfo(item, parseFileInfoMap[item.id]);
            res.push_back(item);
        }
    }
    return res;
}

bool ProjectExplorerManager::SaveProjectExplorer(const ProjectExplorerInfo &projectExplorerInfo, bool isConflict) {
    // db初始化
    if (!InitSystemMemoryDb()) {
        Server::ServerLog::Error("Failed to open database. path:", systemMemoryDbPath);
        return false;
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    db->StartTransaction();
    // 如果存在冲突，则需要清空老项目内容，如果处理失败，则事务回滚
    if (isConflict && !db->DeleteFileMenu(std::vector<std::string>{projectExplorerInfo.projectName}, {})) {
        db->RollbackTransaction();
        return false;
    }

    // 将导入数据进行落盘，如果失败则回滚
    if (!SaveProjectExplorerToDb(projectExplorerInfo.projectName, projectExplorerInfo)) {
        db->RollbackTransaction();
        return false;
    }
    // 提交事务
    db->EndTransaction();
    return true;
}

bool ProjectExplorerManager::SaveProjectExplorerToDb(
    const std::string &projectName, const ProjectExplorerInfo &projectExplorerInfo) {
    if (!db->InsertDuplicateUpdateProject(projectExplorerInfo)) {
        return false;
    }
    std::vector<ProjectExplorerInfo> projectExplorerInfoData =
        db->QueryProjectExplorerData(std::vector<std::string>{projectName}, std::vector<std::string>());
    if (projectExplorerInfoData.empty()) {
        return false;
    }
    std::unordered_map<std::string, int64_t> ukIdMap;
    for (const auto &item : projectExplorerInfoData) {
        if (item.projectType != projectExplorerInfo.projectType) {
            continue;
        }
        ukIdMap[item.projectName + item.fileName] = item.id;
    }

    std::vector<std::shared_ptr<ParseFileInfo>> parseFileInfos;
    std::string uk = projectExplorerInfo.projectName + projectExplorerInfo.fileName;
    if (ukIdMap.find(uk) == ukIdMap.end()) {
        return false;
    }
    int64_t id = ukIdMap[projectExplorerInfo.projectName + projectExplorerInfo.fileName];
    for (auto &item : projectExplorerInfo.fileInfoMap) {
        item.second->projectExplorerId = id;
        parseFileInfos.push_back(item.second);
    }
    return db->InsertDuplicateUpdateParsedFile(parseFileInfos);
}

bool ProjectExplorerManager::InitSystemMemoryDb() {
    if (!db) {
        InitSystemMemoryDbPath(Server::ParamsParser::Instance().GetOption().logPath);
        db = std::make_unique<SystemMemoryDatabase>(mutex);
    }
    // 如果db已经打开，则直接返回
    if (db->IsOpen()) {
        return true;
    }
    // 打开db
    if (!db->OpenDb(systemMemoryDbPath, false)) {
        return false;
    }
    // 判断db是否发生版本变更，如果是，则进行数据表删除操作
    if (db->IsDatabaseVersionChange() && !db->DropAllTable()) {
        return false;
    }
    // db配置并创建表
    if (db->SetConfig() && db->CreateTable() && db->SetDataBaseVersion()) {
        return true;
    }
    return false;
}

bool ProjectExplorerManager::DeleteProjectAndFilePath(
    const std::string &projectName, const std::vector<std::string> &filePathList) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (projectName.empty() && filePathList.empty()) {
        return true;
    }
    if (!InitSystemMemoryDb()) {
        Server::ServerLog::Error("Failed to open database. path:", systemMemoryDbPath);
        return false;
    }

    // 获取项目下所有内容
    std::vector<ProjectExplorerInfo> infos = QueryProjectExplorer(projectName, std::vector<std::string>());
    if (infos.empty()) {
        return true;
    }
    std::vector<int64_t> projectIdList;
    std::vector<int64_t> needDeleteImportFileList;
    std::vector<int64_t> needDeleteParseFileIdList;
    for (auto &project : infos) {
        projectIdList.push_back(project.id);
        bool isNeedDeleteImportData = true;
        for (const auto &item : project.subParseFileInfo) {
            if (filePathList.empty() ||
                std::find(filePathList.begin(), filePathList.end(), item->parseFilePath) != filePathList.end()) {
                auto deleteList = project.GetFileIdsToDelete(item);
                std::copy(deleteList.begin(), deleteList.end(), std::back_inserter(needDeleteParseFileIdList));
            } else if (!filePathList.empty()) {
                // 如果出现了一个导入记录下，有文件没有被删除干净，则不删除该记录
                isNeedDeleteImportData = false;
            }
        }
        if (isNeedDeleteImportData) {
            needDeleteImportFileList.push_back(project.id);
        }
    }

    if (!needDeleteImportFileList.empty()) {
        db->DeleteFileMenu(std::vector<std::string>{projectName}, needDeleteImportFileList);
    }
    if (!db->DeleteParsedFile(projectIdList, needDeleteParseFileIdList)) {
        return false;
    }
    return true;
}

ProjectErrorType ProjectExplorerManager::CheckProjectConflict(
    const std::string &projectName, const std::string &filePath) {
    if (projectName.empty() || filePath.empty()) {
        return ProjectErrorType::OTHER;
    }

    std::pair<std::string, ParserType> parserType = ParserFactory::GetImportType(filePath);
    ParserType allocType = parserType.second;
    std::shared_ptr<ProjectParserBase> factory = ParserFactory::GetProjectParser(allocType);
    ProjectTypeEnum projectTypeEnum = factory->GetProjectType(filePath);

    if (!InitSystemMemoryDb()) {
        Server::ServerLog::Error("Failed to open database. path:", systemMemoryDbPath);
        return ProjectErrorType::OTHER;
    }
    std::vector<ProjectExplorerInfo> infos =
        db->QueryProjectExplorerData(std::vector<std::string>{projectName}, std::vector<std::string>());

    // 校验是否导入的数据是否是历史导入过的
    std::set<std::string> curFilePathList;
    for (const auto &item : infos) {
        curFilePathList.insert(item.fileName);
    }
    if (curFilePathList.count(filePath) != 0) {
        return ProjectErrorType::TRANSFER_PROJECT;
    }
    if (infos.empty()) {
        return ProjectErrorType::NO_ERRORS;
    }
    if (infos[0].projectType < static_cast<int>(ProjectTypeEnum::DB) ||
        infos[0].projectType > static_cast<int>(ProjectTypeEnum::OTHER)) {
        Server::ServerLog::Error(
            "Failed to cast project type. project type is: ", std::to_string(infos[0].projectType));
        return ProjectErrorType::OTHER;
    }
    if (infos[0].importType == "drag") {
        return ProjectErrorType::PROJECT_NAME_CONFLICT;
    }
    ProjectTypeEnum existedProjectType = static_cast<ProjectTypeEnum>(infos[0].projectType);
    if (IsFtraceProfilingCompatible(projectTypeEnum, filePath, existedProjectType, infos[0].fileName)) {
        return ProjectErrorType::NO_ERRORS;
    }
    bool isConflict = isFileConflict(projectTypeEnum, existedProjectType);
    if (isConflict) {
        return ProjectErrorType::PROJECT_NAME_CONFLICT;
    }
    return ProjectErrorType::NO_ERRORS;
}

void ProjectExplorerManager::UpdateProjectDbPath(
    const std::string &projectName, const std::map<std::string, std::vector<std::string>> &dataPathToDbMap) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (projectName.empty()) {
        return;
    }
    if (!InitSystemMemoryDb()) {
        Server::ServerLog::Error("Failed to open database. path:", systemMemoryDbPath);
        return;
    }
    db->StartTransaction();
    for (auto &it : dataPathToDbMap) {
        std::string dbPathStr = StringUtil::join(it.second, ",");
        db->UpdateProjectDbPath(projectName, it.first, dbPathStr);
    }
    db->EndTransaction();
}

void ProjectExplorerManager::InitSystemMemoryDbPath(const std::string &filePath) {
    std::string path = FileUtil::SplicePath(filePath, "system_memory.db");
#ifdef _WIN32
    systemMemoryDbPath = path;
#else
    systemMemoryDbPath = path;
#endif
}

bool ProjectExplorerManager::IsClusterData(const std::string &projectName) {
    std::vector<Global::ProjectExplorerInfo> projectExplorerInfo =
        QueryProjectExplorer(projectName, std::vector<std::string>());
    if (projectExplorerInfo.empty()) {
        return false;
    }
    if (projectExplorerInfo[0].projectType < static_cast<int>(ProjectTypeEnum::DB) ||
        projectExplorerInfo[0].projectType > static_cast<int>(ProjectTypeEnum::OTHER)) {
        Server::ServerLog::Warn(
            "Project type incorrect. project type is: ", std::to_string(projectExplorerInfo[0].projectType));
        return false;
    }
    auto projectTypeEnum = static_cast<ProjectTypeEnum>(projectExplorerInfo[0].projectType);
    if (projectTypeEnum == ProjectTypeEnum::TEXT_CLUSTER || projectTypeEnum == ProjectTypeEnum::DB_CLUSTER) {
        return true;
    }
    return false;
}

bool ProjectExplorerManager::ClearProjectExplorer(const std::vector<std::string> &projectNameList) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (db == nullptr) {
        Server::ServerLog::Warn("System db not init!");
        return true;
    }
    db->StartTransaction();
    // 根据项目名获取对应的id,如果projectNameList为空，代表要清空所有内容，因此这里多个if条件可以减少一次sql查询
    std::vector<int64_t> projectIdList;
    if (!projectNameList.empty()) {
        std::vector<ProjectExplorerInfo> projectInfos =
            db->QueryProjectExplorerData(projectNameList, std::vector<std::string>{});
        for (const auto &item : projectInfos) {
            projectIdList.push_back(item.id);
        }
    }

    // 删除两个表中对应的数据
    if (db->DeleteFileMenu(projectNameList, {}) && db->DeleteParsedFile(projectIdList, {})) {
        // 删除成功，提交事务
        db->EndTransaction();
        Server::ServerLog::Info("Success to clear project explorer.");
        return true;
    }
    // 如果出现其中一个表删除失败的清空，则进行事务回滚
    db->RollbackTransaction();
    Server::ServerLog::Error("Fail to clear project explorer.");
    return false;
}

ProjectTypeEnum ProjectExplorerManager::GetProjectType(const std::vector<ProjectExplorerInfo> &projectInfo) {
    // 外层保证入参不为空
    std::set<ProjectTypeEnum> projectTypeSet;
    for (const auto &item : projectInfo) {
        if (item.projectType < static_cast<int>(ProjectTypeEnum::DB) ||
            item.projectType > static_cast<int>(ProjectTypeEnum::OTHER)) {
            Server::ServerLog::Warn("Ignore project type. project type is: ", std::to_string(item.projectType));
            continue;
        }
        projectTypeSet.insert(static_cast<ProjectTypeEnum>(item.projectType));
    }

    if (projectTypeSet.size() == 1) {
        return *projectTypeSet.begin();
    }

    std::set<ProjectTypeEnum> dbProjectTypeSet = {ProjectTypeEnum::DB_CLUSTER, ProjectTypeEnum::DB};
    std::set<ProjectTypeEnum> dbTypeDifference;
    std::set_difference(dbProjectTypeSet.begin(), dbProjectTypeSet.end(), projectTypeSet.begin(), projectTypeSet.end(),
        std::inserter(dbTypeDifference, dbTypeDifference.begin()));
    if (dbTypeDifference.empty()) {
        return ProjectTypeEnum::DB_CLUSTER;
    }

    std::set<ProjectTypeEnum> textProjectTypeSet = {ProjectTypeEnum::TEXT_CLUSTER, ProjectTypeEnum::TRACE};
    std::set<ProjectTypeEnum> textTypeDifference;
    std::set_difference(textProjectTypeSet.begin(), textProjectTypeSet.end(), projectTypeSet.begin(),
        projectTypeSet.end(), std::inserter(textTypeDifference, textTypeDifference.begin()));
    if (dbTypeDifference.empty()) {
        return ProjectTypeEnum::TEXT_CLUSTER;
    }
    return *projectTypeSet.begin();
}

std::vector<std::shared_ptr<ParseFileInfo>> ProjectExplorerManager::GetClusterFilePath(
    const std::vector<ProjectExplorerInfo> &projectInfo) {
    std::vector<std::shared_ptr<ParseFileInfo>> res;
    for (const auto &item : projectInfo) {
        if (item.projectType == static_cast<int>(ProjectTypeEnum::TEXT_CLUSTER) ||
            item.projectType == static_cast<int>(ProjectTypeEnum::DB_CLUSTER)) {
            auto clusters = item.GetClusterInfos();
            std::copy(clusters.begin(), clusters.end(), std::back_inserter(res));
        }
    }
    return res;
}

void ProjectExplorerManager::RebuildParseFileInfo(
    ProjectExplorerInfo &projectInfo, std::vector<std::shared_ptr<ParseFileInfo>> &parseFileInfos) {
    if (parseFileInfos.empty()) {
        return;
    }
    std::sort(parseFileInfos.begin(), parseFileInfos.end(),
        [](const auto &lh, const auto &rh) { return lh->type < rh->type; });
    std::for_each(parseFileInfos.begin(), parseFileInfos.end(), [&projectInfo](const auto &fileInfo) {
        fileInfo->curDirName = FileUtil::GetFileName(fileInfo->parseFilePath);
        projectInfo.AddSubParseFileInfo(fileInfo);
    });
}

bool ProjectExplorerManager::IsTextMultiCluster(const std::string &projectName) {
    if (projectName.empty()) {
        return false;
    }
    auto projectInfos = QueryProjectExplorer(projectName, {});
    if (projectInfos.empty()) {
        return false;
    }
    bool isText = projectInfos[0].projectType == static_cast<int>(ProjectTypeEnum::TEXT_CLUSTER);
    bool isMultiCluster = projectInfos[0].GetClusterInfos().size() > 1;
    return isText && isMultiCluster;
}

bool ProjectExplorerManager::UpdateParseFileInfo(
    const std::string &projectName, const std::vector<std::shared_ptr<ParseFileInfo>> &parseFileInfo) {
    if (projectName.empty() || parseFileInfo.empty()) {
        return false;
    }
    if (db == nullptr) {
        return false;
    }
    return db->InsertDuplicateUpdateParsedFile(parseFileInfo);
}
}
