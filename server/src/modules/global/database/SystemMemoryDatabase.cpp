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
#include "pch.h"
#include "GlobalDefs.h"
#include "SystemMemoryDatabase.h"

namespace Dic {
namespace Module {
namespace Global {
using namespace Server;
bool SystemMemoryDatabase::SetConfig() { return Database::SetConfig(); }

bool SystemMemoryDatabase::CreateTable() {
    if (!isOpen) {
        ServerLog::Error("Failed to create table. Database is not open.");
        return false;
    }
    if (CheckTableExist(projectExplorerTable)) {
        return true;
    }
    std::string sql = "CREATE TABLE " + projectExplorerTable +
        " ( id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "projectName TEXT, fileName TEXT, projectType INTEGER, importType TEXT, fileId Text, accessTime TEXT, "
        "UNIQUE (projectName, fileName, projectType) );"
        "CREATE TABLE " +
        parseFileInfoTable +
        " ( id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "projectExplorerId INTEGER, parseFilePath TEXT, fileId Text, "
        "subId TEXT,type INTEGER, clusterPath TEXT, "
        "host TEXT, rankId TEXT, deviceId TEXT, projectType INTEGER, "
        "UNIQUE (projectExplorerId, parseFilePath, subId, type));";
    std::unique_lock<std::recursive_mutex> lock(mutex);
    return ExecSql(sql);
}

bool SystemMemoryDatabase::DropTable() {
    std::vector<std::string> tables = {projectExplorerTable};
    std::unique_lock<std::recursive_mutex> lock(mutex);
    return DropSomeTables(tables);
}

bool SystemMemoryDatabase::InsertDuplicateUpdateProject(const ProjectExplorerInfo &projectExplorerInfo) {
    if (projectExplorerInfo.projectName.empty() || projectExplorerInfo.fileName.empty()) {
        ServerLog::Error("Failed to save FileMenuData, params is invalid.");
        return false;
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string sql = "INSERT INTO " + projectExplorerTable +
        "(projectName, fileName, projectType, importType, fileId, accessTime) VALUES(?, ?, ?, ?, ?, ?)";
    sql += " ON CONFLICT(projectName, fileName, projectType) DO UPDATE SET"
           " importType = EXCLUDED.importType,"
           " fileId = EXCLUDED.fileId,"
           " accessTime = EXCLUDED.accessTime;";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to save FileMenuData, prepared statement failed.");
        return false;
    }
    std::string dbPath = StringUtil::join(projectExplorerInfo.dbPath, ",");
    stmt->BindParams(projectExplorerInfo.projectName, projectExplorerInfo.fileName, projectExplorerInfo.projectType,
        projectExplorerInfo.importType, dbPath, projectExplorerInfo.accessTime);
    if (!stmt->Execute()) {
        ServerLog::Error("Failed to save FileMenuData, stmt execute failed.");
        return false;
    }
    return true;
}

bool SystemMemoryDatabase::InsertDuplicateUpdateParsedFile(
    const std::vector<std::shared_ptr<ParseFileInfo>> &parseFileInfoList) {
    if (parseFileInfoList.empty()) {
        return true;
    }
    for (const auto &item : parseFileInfoList) {
        if (item->projectExplorerId == 0) {
            ServerLog::Error("Failed to save FileMenuData, params is invalid.");
            return false;
        }
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string sql = "INSERT OR REPLACE INTO " + parseFileInfoTable +
        "(projectExplorerId, parseFilePath, fileId, subId, type, "
        "clusterPath, host, rankId, deviceId, projectType)"
        " VALUES(?, ?, ?, ? ,? ,? ,? , ? ,?, ?)";
    for (size_t i = 1; i < parseFileInfoList.size(); ++i) {
        sql += ",(?, ?, ?, ? ,? ,? ,? , ? ,?, ?)";
    }
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to save FileMenuData, prepared statement failed.");
        return false;
    }
    for (const auto &item : parseFileInfoList) {
        stmt->BindParams(item->projectExplorerId, item->parseFilePath, item->fileId, item->subId, item->type,
            item->clusterId, item->host, item->rankId, item->deviceId, item->projectType);
    }
    if (!stmt->Execute()) {
        ServerLog::Error("Failed to save FileMenuData, stmt execute failed.");
        return false;
    }
    return true;
}

bool SystemMemoryDatabase::UpdateProjectName(const std::string &oldProjectName, const std::string &newProjectName) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (oldProjectName.empty() || newProjectName.empty()) {
        ServerLog::Error("Failed to update project name, param is invalid.");
        return false;
    }
    if (!CheckTableExist(projectExplorerTable)) {
        ServerLog::Error("Failed to update project name, table is not exist.");
        return false;
    }
    std::string sql = "Update " + projectExplorerTable + " SET projectName = ? WHERE projectName = ?;";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error(
            "Failed to create prepared stmt: oldProjectName=", oldProjectName, ", newProjectName=", newProjectName);
        return false;
    }
    stmt->BindParams(newProjectName, oldProjectName);
    if (!stmt->Execute()) {
        ServerLog::Error(
            "Failed to update project name: oldProjectName=", oldProjectName, ", newProjectName=", newProjectName);
        return false;
    }
    return true;
}

std::vector<ProjectExplorerInfo> SystemMemoryDatabase::QueryProjectExplorerData(
    const std::vector<std::string> &projectNameList, const std::vector<std::string> &fileNameList) {
    std::vector<ProjectExplorerInfo> res;
    std::string sql = "SELECT id, projectName, fileName, projectType, importType, fileId, accessTime FROM " +
        projectExplorerTable + " WHERE 1 = 1";
    if (!projectNameList.empty()) {
        sql += " and projectName in (" + StringUtil::CreateQuestionMarkString(projectNameList.size()) + ")";
    }
    if (!fileNameList.empty()) {
        sql += " and fileName in (" + StringUtil::CreateQuestionMarkString(fileNameList.size()) + ")";
    }
    sql += ";";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to delete file menu, create prepared stmt failed.");
        return res;
    }
    for (const auto &item : projectNameList) {
        stmt->BindParams(item);
    }
    for (const auto &item : fileNameList) {
        stmt->BindParams(item);
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query project explorer data.", stmt->GetErrorMessage());
        return res;
    }
    while (resultSet->Next()) {
        ProjectExplorerInfo info{};
        info.id = resultSet->GetInt64("id");
        info.projectName = resultSet->GetString("projectName");
        info.fileName = resultSet->GetString("fileName");
        info.projectType = resultSet->GetInt64("projectType");
        info.importType = resultSet->GetString("importType");
        info.dbPath = StringUtil::Split(resultSet->GetString("fileId"), ",");
        info.accessTime = resultSet->GetString("accessTime");
        res.emplace_back(info);
    }
    return res;
}

bool SystemMemoryDatabase::DeleteFileMenu(
    const std::vector<std::string> &projectNameList, const std::vector<int64_t> &projectIdList) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!CheckTableExist(projectExplorerTable)) {
        ServerLog::Error("Failed to delete by project name, table is not exist.");
        return false;
    }
    std::string sql = "DELETE FROM " + projectExplorerTable + " WHERE 1 = 1";
    if (!projectNameList.empty()) {
        sql += " and projectName in (" + StringUtil::CreateQuestionMarkString(projectNameList.size()) + ")";
    }
    if (!projectIdList.empty()) {
        sql += " and id in (" + StringUtil::CreateQuestionMarkString(projectIdList.size()) + ")";
    }
    sql += ";";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to delete file menu, create prepared stmt failed.");
        return false;
    }
    for (const auto &item : projectNameList) {
        stmt->BindParams(item);
    }
    for (const auto &item : projectIdList) {
        stmt->BindParams(item);
    }
    if (!stmt->Execute()) {
        ServerLog::Error("Failed to delete file menu, execute sql failed.");
        return false;
    }
    return true;
}

bool SystemMemoryDatabase::DeleteParsedFile(
    const std::vector<int64_t> &projectIdList, const std::vector<int64_t> &idList) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string sql = "DELETE FROM " + parseFileInfoTable + " WHERE 1 = 1";
    if (!projectIdList.empty()) {
        std::string projectListStr = StringUtil::join(projectIdList, ",");
        sql += " And projectExplorerId IN (" + projectListStr + ")";
    }
    if (!idList.empty()) {
        std::string parsePathListStr = StringUtil::join(idList, ",");
        sql += " And id IN (" + parsePathListStr + ")";
    }
    sql += ";";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to delete parsed file");
        return false;
    }
    if (!stmt->Execute()) {
        ServerLog::Error("Failed to delete parsed file");
        return false;
    }
    return true;
}

bool SystemMemoryDatabase::UpdateProjectDbPath(
    const std::string &projectName, const std::string &fileName, const std::string &dbPath) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (projectName.empty() || fileName.empty()) {
        ServerLog::Error("Failed to update ProjectDbPath, param is invalid.");
        return false;
    }
    if (!CheckTableExist(projectExplorerTable)) {
        ServerLog::Error("Failed to update ProjectName failed, table is not exist.");
        return false;
    }
    std::string sql = "Update " + projectExplorerTable + " SET fileId = ? WHERE projectName = ? AND fileName = ?;";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to create prepared stmt: projectName=", projectName, ", fileName=", fileName);
        return false;
    }
    stmt->BindParams(dbPath, projectName, fileName);
    if (!stmt->Execute()) {
        ServerLog::Error("Failed to update project db path: projectName=", projectName, ", fileName=", fileName);
        return false;
    }
    return true;
}

std::map<int64_t, std::vector<std::shared_ptr<ParseFileInfo>>> SystemMemoryDatabase::QueryParseFileInfo(
    const std::vector<int64_t> &projectExplorerIdList, const std::vector<std::string> &parsePathList) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::map<int64_t, std::vector<std::shared_ptr<ParseFileInfo>>> res;
    if (projectExplorerIdList.empty()) {
        return res;
    }
    std::string projectExplorerIdListStr = StringUtil::join(projectExplorerIdList, ", ");
    std::string sql =
        "SELECT * FROM " + parseFileInfoTable + " WHERE projectExplorerId IN (" + projectExplorerIdListStr + ")";
    if (!parsePathList.empty()) {
        std::string parsePathListStr = StringUtil::Join4SqlGroup(parsePathList);
        sql += " AND parseFilePath IN (" + parsePathListStr + ")";
    }
    sql += ";";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to create prepared stmt for query parse file info.");
        return res;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query parse file info.", stmt->GetErrorMessage());
        return res;
    }

    while (resultSet->Next()) {
        auto info = std::make_shared<ParseFileInfo>();
        info->id = static_cast<int64_t>(resultSet->GetUint64("id"));
        info->projectExplorerId = static_cast<int64_t>(resultSet->GetUint64("projectExplorerId"));
        info->parseFilePath = resultSet->GetString("parseFilePath");
        info->fileId = resultSet->GetString("fileId");
        info->subId = resultSet->GetString("subId");
        info->type = static_cast<ParseFileType>(resultSet->GetInt64("type"));
        info->clusterId = resultSet->GetString("clusterPath");
        info->host = resultSet->GetString("host");
        info->rankId = resultSet->GetString("rankId");
        info->deviceId = resultSet->GetString("deviceId");
        info->projectType = resultSet->GetInt64("projectType");
        res[info->projectExplorerId].push_back(info);
    }
    return res;
}

}
}
}
