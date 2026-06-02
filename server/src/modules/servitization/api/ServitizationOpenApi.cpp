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
#include <queue>
#include "FileUtil.h"
#include "IEProtocolEvent.h"
#include "WsSender.h"
#include "ServitizationOpenApi.h"
#include "ParserStatusManager.h"
namespace Dic::Module::IE {
bool ServitizationOpenApi::Parse(const std::unordered_map<std::string, std::string> &inputs) {
    Dic::Module::Timeline::ParserStatusManager::Instance().WaitStartParse();
    bool res = false;
    for (const auto &item : inputs) {
        std::string dir = FileUtil::GetParentPath(item.second);
        res = ParseDir(dir, item.first);
    }
    return res;
}

std::vector<TaskInfo> ServitizationOpenApi::ComputeTaskInfo(const std::string &path) {
    if (ValidIEFile(path)) {
        std::vector<TaskInfo> res;
        std::string folder = FileUtil::GetParentPath(path);
        std::string fileId = context->ComputeFileIdByFolder(folder);
        TaskInfo taskInfo = {fileId, path};
        res.emplace_back(taskInfo);
        return res;
    }
    const int maxDepth = 5;
    std::queue<std::pair<std::string, int>> queue;
    queue.emplace(path, 0);
    std::vector<std::string> validFiles;
    while (!queue.empty()) {
        auto [node, depth] = queue.front();
        queue.pop();
        if (depth > maxDepth) {
            break;
        }
        std::vector<std::string> folders;
        std::vector<std::string> files;
        FileUtil::FindFolders(node, folders, files);
        for (const auto &item : files) {
            std::string absPath = FileUtil::SplicePath(node, item);
            if (ValidIEFile(absPath)) {
                validFiles.emplace_back(absPath);
            }
        }
        for (const auto &item : folders) {
            std::string absPath = FileUtil::SplicePath(node, item);
            queue.emplace(absPath, depth + 1);
        }
    }
    std::vector<TaskInfo> res;
    for (const auto &item : validFiles) {
        std::string fileId = context->ComputeFileIdByFolder(FileUtil::GetParentPath(item));
        TaskInfo taskInfo = {fileId, item};
        res.emplace_back(taskInfo);
    }
    return res;
}

void ServitizationOpenApi::Reset() { context->Reset(); }

bool ServitizationOpenApi::ValidIEFile(const std::string &path) {
    if (FileUtil::IsFolder(path)) {
        return false;
    }
    std::string fileName = FileUtil::GetFileName(path);
    if (fileName == IEFileName || fileName == MS_SERVICE_PARSED_NAME) {
        return true;
    }
    return false;
}

void ServitizationOpenApi::ParseSingleFile(const std::string &filePath, const std::string &fileId) {
    auto event = std::make_unique<Protocol::ParseStatisticCompletedEvent>();
    event->moduleName = Protocol::MODULE_IE;
    event->result = true;
    std::vector<std::string> fileIds;
    fileIds.push_back(fileId);
    event->rankIds = fileIds;
    event->fileId = filePath;
    SendEvent(std::move(event));
}

bool ServitizationOpenApi::CreateCurve(const std::string &fileId, const std::string &curve) {
    if (curve.find("_curve' AS ") == std::string::npos) {
        return false;
    }
    context->ExecuteScript(fileId, curve);
    return true;
}

bool ServitizationOpenApi::ParseDir(const std::string &dir, const std::string &fileId) {
    std::string targetFile = FileUtil::SplicePath(dir, IEFileName);
    if (FileUtil::CheckFilePathExist(targetFile)) {
        context->InitDataBase(fileId, targetFile);
        ParseSingleFile(targetFile, fileId);
        return true;
    }
    targetFile = FileUtil::SplicePath(dir, MS_SERVICE_PARSED_NAME);
    if (FileUtil::CheckFilePathExist(targetFile)) {
        std::vector<std::string> files;
        std::vector<std::string> folders;
        FileUtil::FindFolders(dir, folders, files);
        std::vector<std::string> distributeFiles;
        for (const auto &item : files) {
            if (item != MS_SERVICE_PARSED_NAME && StringUtil::StartWith(item, "ms_service_") &&
                StringUtil::EndWith(item, ".db")) {
                distributeFiles.emplace_back(item);
            }
        }
        context->InitDataBase(fileId, targetFile);
        auto database = context->GetDatabase(fileId);
        std::string dropSql = "PRAGMA journal_mode = WAL;PRAGMA synchronous = OFF;PRAGMA busy_timeout = 5000; ";
        dropSql += "DROP TABLE IF EXISTS slice;DROP TABLE IF EXISTS counter;DROP TABLE IF EXISTS thread;";
        database->ExecSql(dropSql);
        AttachDb(dir, distributeFiles, database);
        std::string attachSql;
        std::string detachSqls;
        for (const auto &item : distributeFiles) {
            std::string distributePath = FileUtil::SplicePath(dir, item);
            if (!FileUtil::CheckPathSecurity(distributePath, CHECK_FILE_READ)) {
                continue;
            }
            std::string alias = item.substr(0, item.size() - std::string(".db").size());
            if (!StringUtil::CheckSqlValid(alias)) {
                continue;
            }
            // LCOV_EXCL_BR_START
            attachSql += " CREATE TABLE IF NOT EXISTS main.slice AS SELECT * FROM '" + alias + "'.slice WHERE 0; ";
            attachSql += " CREATE TABLE IF NOT EXISTS main.counter AS SELECT * FROM '" + alias + "'.counter WHERE 0; ";
            attachSql += " CREATE TABLE IF NOT EXISTS main.thread AS SELECT * FROM '" + alias + "'.thread WHERE 0; ";
            attachSql += " INSERT INTO main.slice SELECT * FROM '" + alias + "'.slice; ";
            attachSql += " INSERT INTO main.counter SELECT * FROM '" + alias + "'.counter; ";
            attachSql += " INSERT INTO main.thread SELECT * FROM '" + alias + "'.thread; ";
            detachSqls += " DETACH '" + alias + "'; ";
            // LCOV_EXCL_BR_STOP
        }
        std::string script = "BEGIN IMMEDIATE;" + attachSql + "COMMIT;" + detachSqls;
        database->ExecSql(script);
        ParseSingleFile(targetFile, fileId);
        return true;
    }
    return false;
}

void ServitizationOpenApi::AttachDb(
    const std::string &dir, std::vector<std::string> &distributeFiles, std::shared_ptr<Database> &database) {
    std::string attachSql;
    for (const auto &item : distributeFiles) {
        AttachSingleDb(dir, item, database);
    }
}

void ServitizationOpenApi::AttachSingleDb(
    const std::string &dir, const std::string &distributeFile, std::shared_ptr<Database> &database) {
    std::string distributePath = FileUtil::SplicePath(dir, distributeFile);
    if (!FileUtil::CheckPathSecurity(distributePath, CHECK_FILE_READ)) {
        return;
    }
    std::string alias = distributeFile.substr(0, distributeFile.size() - 3);
    if (!StringUtil::CheckSqlValid(alias)) {
        return;
    }
    std::string attachSql = " ATTACH DATABASE ? AS '" + alias + "'; ";
    auto stmt = database->CreatPreparedStatement(attachSql);
    if (!TryOpt(stmt, "Attach single db failed to prepare sql, alias is: " + alias)) {
        return;
    }
    stmt->BindParams(StringUtil::ToUtf8Str(distributePath));
    stmt->Execute();
}
} // namespace Dic::Module::IE
