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
#include "ServerLog.h"
#include "TextMemoryDataBase.h"
#include "DbTraceDataBase.h"
#include "DbMemoryDataBase.h"
#include "DbSummaryDataBase.h"
#include "DbPlatformDataBase.h"
#include "DbClusterDataBase.h"
#include "BaselineManager.h"
#include "TrackInfoManager.h"
#include "DataBaseManager.h"
#include "SearchSliceCacheManager.h"
#include "MemSnapshotStateCache.h"

// clang-format off
namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
using namespace FullDb;
DataBaseManager &DataBaseManager::Instance()
{
    static DataBaseManager instance;
    return instance;
}

bool DataBaseManager::CreateTraceConnectionPool(const std::string &rankId, const std::string &dbPath)
{
    const static unsigned int CPU_CORE_COUNT = SystemUtil::GetCpuCoreCount();
    std::unique_lock<std::recursive_mutex> lock(mutex);
    databasePathSet.emplace(dbPath);
    SetRankIdFileIdMapping(rankId, dbPath);
    DataType curDataType = GetDataType(dbPath);
    std::string fileId = dbPath;
    if (traceDatabaseMap.count(fileId) == 0) {
        std::recursive_mutex &dbMutex = GetDbMutex(fileId);
        std::shared_ptr<DBConnectionPool<VirtualTraceDatabase>> conn;
        switch (curDataType) {
            case DataType::DB:
                conn = std::make_shared<DBConnectionPool<VirtualTraceDatabase>>(dbPath,
                    [&dbMutex]() { return new FullDb::DbTraceDataBase(dbMutex); });
                break;
            case DataType::TEXT:
            default:
                conn = std::make_shared<DBConnectionPool<VirtualTraceDatabase>>(dbPath,
                    [&dbMutex]() { return new TextTraceDatabase(dbMutex); }
                );
                break;
        }
        conn->SetMaxActiveCount(CPU_CORE_COUNT);
        traceDatabaseMap.emplace(fileId, std::move(conn));
        return true;
    }
    ServerLog::Error("The file id has a connection. id:", fileId, ", old path:",
        traceDatabaseMap.at(fileId)->GetDbPath(), ", new path:", dbPath);
    return false;
}

std::shared_ptr<VirtualTraceDatabase> DataBaseManager::GetTraceDatabaseByRankId(const std::string &rankId)
{
    std::string fileId = GetFileIdByRankId(rankId);
    return GetTraceDatabaseByFileId(fileId);
}

std::shared_ptr<VirtualTraceDatabase> DataBaseManager::GetTraceDatabaseByFileId(const std::string &fileId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    auto it = traceDatabaseMap.find(fileId);
    if (it == traceDatabaseMap.end() && dbFilePathMap.count(fileId) != 0) {
        it = traceDatabaseMap.find(dbFilePathMap[fileId]);
    }
    if (it == traceDatabaseMap.end()) {
        ServerLog::Error("Can't find connection pool. fileId:", fileId);
        return nullptr;
    }
    auto ptr = it->second->GetConnection();
    return ptr;
}

std::string DataBaseManager::GetRankIdByFileId(const std::string &fileId)
{
    return fileIdToRankIdMap[fileId];
}

std::shared_ptr<Summary::VirtualSummaryDataBase> DataBaseManager::GetSummaryDatabaseByRankId(const std::string &rankId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::vector<std::string> ids;
    std::shared_ptr<Summary::VirtualSummaryDataBase> db = nullptr;
    std::string fileId = GetFileIdByRankId(rankId);
    return GetSummaryDataBaseByFileId(fileId);
}

std::shared_ptr<Summary::VirtualSummaryDataBase> DataBaseManager::GetSummaryDataBaseByFileId(const std::string &fileId)
{
    std::unique_lock lock(mutex);
    auto it = summaryDatabaseMap.find(fileId);
    if (it == summaryDatabaseMap.end() && dbFilePathMap.count(fileId) != 0) {
        it = summaryDatabaseMap.find(dbFilePathMap[fileId]);
    }
    if (it == summaryDatabaseMap.end()) {
        ServerLog::Error("Can't find summary database. FileId:", fileId);
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<Summary::VirtualSummaryDataBase> DataBaseManager::CreateSummaryDatabase(const std::string &rankId,
                                                                                        const std::string &dbPath)
{
    std::unique_lock lock(mutex);
    databasePathSet.emplace(dbPath);
    DataType curDataType = GetDataType(dbPath);
    std::string fileId = dbPath;
    SetRankIdFileIdMapping(rankId, fileId);
    if (summaryDatabaseMap.count(fileId) == 0) {
        std::recursive_mutex &dbMutex = GetDbMutex(fileId);
        if (curDataType == DataType::TEXT) {
            summaryDatabaseMap.emplace(fileId, std::make_shared<Summary::TextSummaryDataBase>(dbMutex));
        } else if (curDataType == DataType::DB) {
            summaryDatabaseMap.emplace(fileId, std::make_shared<FullDb::DbSummaryDataBase>(dbMutex));
        }
    }
    return summaryDatabaseMap[fileId];
}

std::shared_ptr<Memory::VirtualMemoryDataBase> DataBaseManager::CreateMemoryDataBase(const std::string &rankId,
                                                                                     const std::string &dbPath)
{
    std::unique_lock lock(mutex);
    databasePathSet.emplace(dbPath);
    DataType curDataType = GetDataType(dbPath);
    std::string fileId = dbPath;
    SetRankIdFileIdMapping(rankId, fileId);
    if (memoryDatabaseMap.count(fileId) == 0) {
        std::recursive_mutex &dbMutex = GetDbMutex(fileId);
        switch (curDataType) {
            case DataType::DB:
                memoryDatabaseMap.emplace(fileId, std::make_unique<FullDb::DbMemoryDataBase>(dbMutex));
                break;
            case DataType::TEXT:
            default:
                memoryDatabaseMap.emplace(fileId, std::make_unique<Memory::TextMemoryDataBase>(dbMutex));
                break;
        }
    }
    auto res = memoryDatabaseMap[fileId];
    if (!res->IsOpen()) {
        res->SetDbPath(dbPath);
    }
    return memoryDatabaseMap[fileId];
}

std::shared_ptr<Memory::VirtualMemoryDataBase> DataBaseManager::GetMemoryDatabaseByRankId(const std::string &rankId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string fileId = GetFileIdByRankId(rankId);
    return GetMemoryDatabaseByFileId(fileId);
}

std::shared_ptr<Memory::VirtualMemoryDataBase> DataBaseManager::GetMemoryDatabaseByFileId(const std::string &fileId)
{
    std::unique_lock lock(mutex);
    auto it = memoryDatabaseMap.find(fileId);
    if (it == memoryDatabaseMap.end() && dbFilePathMap.count(fileId) != 0) {
        it = memoryDatabaseMap.find(dbFilePathMap[fileId]);
    }
    if (it == memoryDatabaseMap.end()) {
        ServerLog::Error("Can't find memory database. FileId:", fileId);
        return nullptr;
    }
    return it->second;
}

void DataBaseManager::ReleaseDatabaseByRankId(const std::string &rankId)
{
    std::string fileId = GetFileIdByRankId(rankId);
    return ReleaseDatabaseByFileId(fileId);
}

void DataBaseManager::ReleaseDatabaseByFileId(const std::string &fileId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    auto traceDataBase = traceDatabaseMap.find(fileId);
    if (traceDataBase != traceDatabaseMap.end()) {
        traceDatabaseMap.erase(traceDataBase);
    }
    auto memory = memoryDatabaseMap.find(fileId);
    if (memory != memoryDatabaseMap.end()) {
        memoryDatabaseMap.erase(memory);
    }
    auto summary = summaryDatabaseMap.find(fileId);
    if (summary != summaryDatabaseMap.end()) {
        summaryDatabaseMap.erase(summary);
    }
    Dic::Module::MemSnapshot::MemSnapshotStateCache::ClearData(fileId);
    // 清理对应的缓存
    std::string rankId = GetRankIdByFileId(fileId);
    if (!rankId.empty()) {
        SearchSliceCacheManager::Instance().clear(rankId);
    }
}

bool DataBaseManager::HasRankId(DatabaseType type, const std::string &rankId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string fileId = GetFileIdByRankId(rankId);
    bool result = false;
    switch (type) {
        case DatabaseType::TRACE:
            result = traceDatabaseMap.find(fileId) != traceDatabaseMap.end();
            break;
        case DatabaseType::SUMMARY:
            result = summaryDatabaseMap.find(fileId) != summaryDatabaseMap.end();
            break;
        case DatabaseType::MEMORY:
            result = memoryDatabaseMap.find(fileId) != memoryDatabaseMap.end();
            break;
        default:
            break;
    }
    return result;
}

std::vector<DBConnectionPool<VirtualTraceDatabase> *> DataBaseManager::GetAllTraceDatabase()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::vector<DBConnectionPool<VirtualTraceDatabase> *> traceDatabases;
    for (auto &traceDatabase : traceDatabaseMap) {
        traceDatabases.emplace_back(traceDatabase.second.get());
    }
    return traceDatabases;
}

std::vector<Memory::VirtualMemoryDataBase *> DataBaseManager::GetAllMemoryDatabase()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::vector<Memory::VirtualMemoryDataBase *> databases;
    for (auto &databaseMap : memoryDatabaseMap) {
        databases.emplace_back(databaseMap.second.get());
    }
    return databases;
}

std::vector<Summary::VirtualSummaryDataBase *> DataBaseManager::GetAllSummaryDatabase()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::vector<Summary::VirtualSummaryDataBase *> databases;
    for (auto &databaseMap : summaryDatabaseMap) {
        databases.emplace_back(databaseMap.second.get());
    }
    return databases;
}


std::shared_ptr<Platform::VirtualPlatformDataBase> DataBaseManager::CreatePlatformDataBase(const std::string &rankId,
                                        const std::string &dbPath)
{
    std::unique_lock lock(mutex);
    std::string fileId = dbPath;
    databasePathSet.emplace(dbPath);
    SetRankIdFileIdMapping(rankId, fileId);

    if (platformDatabaseMap.count(fileId) == 0) {
        std::recursive_mutex &dbMutex = GetDbMutex(fileId);

        platformDatabaseMap.emplace(
            fileId,
            std::make_unique<FullDb::DbPlatformDataBase>(dbMutex)
        );
    }

    ServerLog::Info("[Platform] Database created for rankId: ", rankId, fileId);
    return platformDatabaseMap[fileId];
}

std::shared_ptr<Platform::VirtualPlatformDataBase> DataBaseManager::GetPlatformDatabaseByRankId(const std::string &rankId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string fileId = GetFileIdByRankId(rankId);
    auto it = platformDatabaseMap.find(fileId);
    if (it == platformDatabaseMap.end() && dbFilePathMap.count(fileId) != 0) {
        it = platformDatabaseMap.find(dbFilePathMap[fileId]);
    }
    if (it == platformDatabaseMap.end()) {
        ServerLog::Error("Can't find platform database. FileId:", fileId);
        return nullptr;
    }
    return it->second;
}

std::vector<Platform::VirtualPlatformDataBase *> DataBaseManager::GetAllPlatformDatabase()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::vector<Platform::VirtualPlatformDataBase *> databases;
    for (auto &databaseMap : platformDatabaseMap) {
        databases.emplace_back(databaseMap.second.get());
    }
    return databases;
}

void DataBaseManager::Clear()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    traceDatabaseMap.clear();
    memoryDatabaseMap.clear();
    summaryDatabaseMap.clear();
    platformDatabaseMap.clear();
    clusterDatabaseMap.clear();
    clusterProject2DbPathMap.clear();
    dbMutexMap.clear();
    dbFilePathMap.clear();
    host2DbPath.clear();
    databasePathSet.clear();
    memScopeDatabaseMap.clear();
    rankId2FileIdMap.clear();
    dataTypeMap.clear();
    fileTypeMap.clear();
    rankIdToDeviceIdMap.clear();
    // 清理所有缓存
    SearchSliceCacheManager::Instance().clearAll();
}

void DataBaseManager::Clear(DatabaseType type)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    switch (type) {
        case DatabaseType::TRACE:
            traceDatabaseMap.clear();
            break;
        case DatabaseType::SUMMARY:
            summaryDatabaseMap.clear();
            break;
        case DatabaseType::MEMORY:
            memoryDatabaseMap.clear();
            break;
        case DatabaseType::MEM_SCOPE:
            memScopeDatabaseMap.clear();
            break;
        case DatabaseType::MEM_SNAPSHOT:
            memSnapshotDatabaseMap.clear();
            break;
        case DatabaseType::PLATFORM:
            platformDatabaseMap.clear();
            break;
        default:
            break;
    }
}

void DataBaseManager::EraseClusterDb(const std::string &uniqueKey)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (clusterDatabaseMap.count(uniqueKey) != 0) {
        clusterDatabaseMap.erase(clusterDatabaseMap.find(uniqueKey));
        // erase clusterProject2DbPathMap on value of uniqueKey
        auto it = std::find_if(clusterProject2DbPathMap.begin(), clusterProject2DbPathMap.end(),
            [&uniqueKey](const auto &pair) {
            return pair.second == uniqueKey;
            });
        if (it != clusterProject2DbPathMap.end()) {
            clusterProject2DbPathMap.erase(it);
        }
    }
}

void DataBaseManager::ClearClusterDb()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    clusterDatabaseMap.clear();
    clusterProject2DbPathMap.clear();
}

/**
 * 使用连接池时，需传入db文件路径, 此处设置project->db路径映射，是为了兼容旧有GetClusterDatabase通过project路径调用数据库连接的逻辑
 * @param projectPath 导入项目文件路径
 * @param dbPath 集群db文件路径，用作唯一键
 */
void DataBaseManager::SetClusterProjectDbPathMapping(const std::string &projectPath, const std::string &dbPath)
{
    clusterProject2DbPathMap[projectPath] = dbPath;
}

/**
 * 创建集群db连接池（如果连接池已存在, 则清除重新创建）
 * @param projectPath 导入项目文件路径
 * @param dbPath 集群db文件路径，用作唯一键
 * @param type 数据类型，DB or TEXT
 * @return
 */
void DataBaseManager::CreateClusterConnectionPool(const std::string &projectPath, const std::string &dbPath,
                                                  DataType type)
{
    const static unsigned int CPU_CORE_COUNT = SystemUtil::GetCpuCoreCount();
    std::unique_lock<std::recursive_mutex> lock(mutex);
    // 如果连接池已存在, 则清除重新创建
    if (clusterDatabaseMap.count(dbPath) != 0) {
        EraseClusterDb(dbPath);
    }
    SetClusterProjectDbPathMapping(projectPath, dbPath);
    std::recursive_mutex &dbMutex = GetDbMutex(dbPath);
    std::shared_ptr<DBConnectionPool<VirtualClusterDatabase>> conn;
    switch (type) {
        case DataType::DB:
            conn = std::make_shared<DBConnectionPool<VirtualClusterDatabase>>(dbPath,
                [&dbMutex]() { return new FullDb::DbClusterDataBase(dbMutex); });
            break;
        case DataType::TEXT:
        default:
            conn = std::make_shared<DBConnectionPool<VirtualClusterDatabase>>(dbPath,
                [&dbMutex]() { return new TextClusterDatabase(dbMutex); }
            );
            break;
    }
    conn->SetMaxActiveCount(CPU_CORE_COUNT);
    clusterDatabaseMap.emplace(dbPath, std::move(conn));
}

/**
 * 根据唯一键获取集群db（会返回空指针，如不存在会返回空指针，外层需做好空指针校验）
 *
 * @param projectPath 导入项目文件路径
 * @return
 */
std::shared_ptr<VirtualClusterDatabase> DataBaseManager::GetClusterDatabase(const std::string &projectPath)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string uniqueKey = clusterProject2DbPathMap[projectPath];
    auto it = clusterDatabaseMap.find(uniqueKey);
    if (it == clusterDatabaseMap.end()) {
        return nullptr;
    }
    auto ptr = it->second->GetConnection();
    return ptr;
}

std::vector<std::shared_ptr<VirtualClusterDatabase>> DataBaseManager::GetAllClusterDatabase()
{
    std::vector<std::shared_ptr<VirtualClusterDatabase>> res;
    for (auto& [key, value]: clusterDatabaseMap) {
        (void)(key);
        res.push_back(value->GetConnection());
    }
    return res;
}

std::vector<std::string> DataBaseManager::GetAllRankId()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::vector<std::string> traceFileId;
    for (auto &traceDatabase : traceDatabaseMap) {
        traceFileId.emplace_back(fileIdToRankIdMap[traceDatabase.first]);
    }
    return traceFileId;
}

std::string DataBaseManager::GetDbPathByRankId(const std::string &rankId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string fileId = GetFileIdByRankId(rankId);
    auto it = traceDatabaseMap.find(fileId);
    if (it == traceDatabaseMap.end()) {
        ServerLog::Error("Can't find db path for rank: %", rankId);
        return "";
    }
    return it->second->GetDbPath();
}

std::shared_ptr<VirtualTraceDatabase> DataBaseManager::GetTraceDatabaseWithOutHost(const std::string &rankId)
{
    std::vector<std::string> ids;
    for (const auto &hostInfo : host2DbPath) {
        ids.push_back(StringUtil::ReplaceFirst(hostInfo.first, "Host", rankId));
    }
    std::shared_ptr<VirtualTraceDatabase> database;
    for (const auto &id : ids) {
        database = GetTraceDatabaseByRankId(id);
        if (database != nullptr) {
            break;
        }
    }
    return database;
}

std::recursive_mutex &DataBaseManager::GetDbMutex(const std::string &fileId)
{
    return dbMutexMap[fileId];
}

DataType DataBaseManager::GetDataType(const std::string &fileId)
{
    if (fileId.empty()) {
        Server::ServerLog::Error("Failed to get data type: fileId is empty.");
    }
    if (dataTypeMap.find(fileId) == dataTypeMap.end()) {
        return DataType::TEXT;
    }
    return dataTypeMap[fileId];
}
void DataBaseManager::SetDataType(DataType type, const std::string &fileId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    dataTypeMap[fileId] = type;
}

FileType DataBaseManager::GetFileType(const std::string &fileId)
{
    if (fileTypeMap.find(fileId) == fileTypeMap.end()) {
        return FileType::PYTORCH;
    }
    return fileTypeMap[fileId];
}

FileType DataBaseManager::GetFileTypeByRankId(const std::string &rankId)
{
    auto fileId = GetFileIdByRankId(rankId);
    return GetFileType(fileId);
}
void DataBaseManager::SetFileType(FileType type, const std::string &fileId)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    fileTypeMap[fileId] = type;
}

void DataBaseManager::SetDbPathMapping(const std::string &rankId, const std::string &dbPath,
                                       const std::string &hostId)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    rankId2FileIdMap[rankId] = dbPath;
    dbFilePathMap[rankId] = dbPath;
    host2DbPath[hostId].push_back(dbPath);
}

bool DataBaseManager::ResetBaseline(bool force)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for (const auto &item : memoryBaselineDatabaseMap) {
        if (item.second != nullptr) {
            item.second->CloseDb();
        }
    }
    memoryBaselineDatabaseMap.clear();
    for (auto &item : summaryBaselineDatabaseMap) {
        if (item.second != nullptr) {
            item.second->CloseDb();
        }
    }
    summaryBaselineDatabaseMap.clear();
    auto baselineRankId = BaselineManager::Instance().GetBaselineId();
    if (force) {
        auto baseFileId = GetFileIdByRankId(baselineRankId);
        traceDatabaseMap.erase(baseFileId);
        rankId2FileIdMap.erase(baselineRankId);
        fileIdToRankIdMap.erase(baseFileId);
        databasePathSet.erase(baseFileId);
    }
    return true;
}

bool DataBaseManager::IsContainDatabasePath(const std::string &databasePath)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return (databasePathSet.count(databasePath) > 0);
}

std::shared_ptr<FullDb::MemScopeDatabase> DataBaseManager::GetMemScopeDatabase(const std::string &fileId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (fileId.empty() && !memScopeDatabaseMap.empty()) {
        return memScopeDatabaseMap.begin()->second;
    }
    // 未找到时默认创建新db
    if (memScopeDatabaseMap.find(fileId) == memScopeDatabaseMap.end()) {
        std::recursive_mutex &dbMutex = GetDbMutex(fileId);
        memScopeDatabaseMap.emplace(fileId, std::make_unique<FullDb::MemScopeDatabase>(dbMutex));
    }
    return memScopeDatabaseMap[fileId];
}

std::shared_ptr<FullDb::MemSnapshotDatabase> DataBaseManager::GetMemSnapshotDatabase(const std::string& fileId)
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (fileId.empty() && !memSnapshotDatabaseMap.empty()) {
        return memSnapshotDatabaseMap.begin()->second;
    }
    // 未找到时默认创建新实例
    if (memSnapshotDatabaseMap.find(fileId) == memSnapshotDatabaseMap.end()) {
        std::recursive_mutex &dbMutex = GetDbMutex(fileId);
        memSnapshotDatabaseMap.emplace(fileId, std::make_unique<FullDb::MemSnapshotDatabase>(dbMutex));
    }
    return memSnapshotDatabaseMap[fileId];
}

std::vector<FullDb::MemScopeDatabase*> DataBaseManager::GetAllMemScopeDatabase()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::vector<FullDb::MemScopeDatabase*> memScopeDatabases;
    memScopeDatabases.reserve(memScopeDatabaseMap.size());
    for (auto& database : memScopeDatabaseMap) { memScopeDatabases.emplace_back(database.second.get()); }
    return memScopeDatabases;
}

std::vector<FullDb::MemSnapshotDatabase*> DataBaseManager::GetAllMemSnapshotDatabase()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::vector<FullDb::MemSnapshotDatabase*> memSnapshotDatabases;
    memSnapshotDatabases.reserve(memSnapshotDatabaseMap.size());
    for (auto& database : memSnapshotDatabaseMap) { memSnapshotDatabases.emplace_back(database.second.get()); }
    return memSnapshotDatabases;
}

std::string DataBaseManager::GetAnyTraceDatabaseId()
{
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (traceDatabaseMap.empty()) {
        return "";
    }
    return traceDatabaseMap.begin()->first;
}
void DataBaseManager::SetRankIdFileIdMapping(const std::string &rankId, const std::string &fileId)
{
    rankId2FileIdMap[rankId] = fileId;
    fileIdToRankIdMap[fileId] = rankId;
}
std::string DataBaseManager::GetFileIdByRankId(const std::string &rankId) const
{
    if (rankId2FileIdMap.find(rankId) == rankId2FileIdMap.end()) {
        return "";
    }
    return rankId2FileIdMap.at(rankId);
}
void DataBaseManager::UpdateRankIdToDeviceId(const std::string &fileId,
                                             const std::string &rankId,
                                             const std::string &deviceId)
{
    if (fileId.empty() || rankId.empty()) {
        return;
    }
    std::string deviceIdTmp = deviceId;
    if (deviceId.empty()) {
        if (auto pos = rankId.find("Baseline_"); pos != std::string::npos) {
            deviceIdTmp = rankId.substr(pos + strlen("Baseline_"));
        }
    }
    rankIdToDeviceIdMap[fileId + rankId] = deviceIdTmp;
}

std::string DataBaseManager::GetDeviceIdFromRankId(const std::string &rankId)
{
    std::string fileId = GetFileIdByRankId(rankId);
    const auto key = fileId + rankId;
    if (rankIdToDeviceIdMap.count(key) == 0) {
        ServerLog::Warn("Don't find deviceId in rankIdToDeviceIdMap");
        return "";
    }
    return rankIdToDeviceIdMap[key];
}

std::shared_ptr<Summary::VirtualSummaryDataBase> DataBaseManager::GetSummaryDatabaseWithCluster(
    const std::string &cluster, const std::string &rankId)
{
    auto clusterId = FileUtil::GetFileName(cluster);
    auto rankIdWithHost = TrackInfoManager::Instance().GetRankInCluster(clusterId, rankId);
    return GetSummaryDatabaseByRankId(rankIdWithHost);
}

std::shared_ptr<VirtualTraceDatabase> DataBaseManager::GetTraceDatabaseInCluster(
    const std::string &clusterPath, const std::string &rankId)
{
    auto clusterId = FileUtil::GetFileName(clusterPath);
    auto rankIdWithHost = TrackInfoManager::Instance().GetRankInCluster(clusterId, rankId);
    return GetTraceDatabaseByRankId(rankIdWithHost);
}
DataType DataBaseManager::GetDataTypeByRank(const std::string &rankId)
{
    auto fileId = GetFileIdByRankId(rankId);
    return dataTypeMap[fileId];
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
// clang-format on
