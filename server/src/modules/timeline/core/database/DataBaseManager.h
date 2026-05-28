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

#ifndef PROFILER_SERVER_DATABASEMANAGER_H
#define PROFILER_SERVER_DATABASEMANAGER_H

#include <map>
#include <memory>
#include <mutex>
#include <unordered_set>
#include "DBConnectionPool.h"
#include "TextTraceDatabase.h"
#include "DbTraceDataBase.h"
#include "TextClusterDatabase.h"
#include "VirtualMemoryDataBase.h"
#include "MemScopeDatabase.h"
#include "MemSnapshotDatabase.h"
#include "KernelParse.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Module::FullDb;
enum class DatabaseType { TRACE, SUMMARY, MEMORY, MEM_SCOPE, MEM_SNAPSHOT };
enum class DataType { TEXT, DB };
enum class FileType { MS_PROF, PYTORCH };
class DataBaseManager {
  public:
    static DataBaseManager &Instance();
    DataBaseManager(const DataBaseManager &) = delete;
    DataBaseManager &operator=(const DataBaseManager &) = delete;
    DataBaseManager(DataBaseManager &&) = delete;
    DataBaseManager &operator=(DataBaseManager &&) = delete;

    bool CreateTraceConnectionPool(const std::string &rankId, const std::string &dbPath);
    std::shared_ptr<VirtualTraceDatabase> GetTraceDatabaseByRankId(const std::string &rankId);
    std::shared_ptr<VirtualTraceDatabase> GetTraceDatabaseByFileId(const std::string &fileId);

    std::vector<DBConnectionPool<VirtualTraceDatabase> *> GetAllTraceDatabase();
    std::vector<std::string> GetAllRankId();
    void Clear();
    void Clear(DatabaseType type);
    void EraseClusterDb(const std::string &uniqueKey);
    void ClearClusterDb();
    void ReleaseDatabaseByRankId(const std::string &rankId);
    void ReleaseDatabaseByFileId(const std::string &fileId);
    bool HasRankId(DatabaseType type, const std::string &rankId);
    void CreateClusterConnectionPool(const std::string &projectPath, const std::string &dbPath, DataType type);
    std::shared_ptr<VirtualClusterDatabase> GetClusterDatabase(const std::string &uniqueKey);
    std::vector<std::shared_ptr<VirtualClusterDatabase>> GetAllClusterDatabase();

    std::shared_ptr<Memory::VirtualMemoryDataBase> CreateMemoryDataBase(
        const std::string &rankId, const std::string &dbPath);
    std::shared_ptr<Memory::VirtualMemoryDataBase> GetMemoryDatabaseByRankId(const std::string &rankId);
    std::shared_ptr<Memory::VirtualMemoryDataBase> GetMemoryDatabaseByFileId(const std::string &fileId);

    std::vector<Memory::VirtualMemoryDataBase *> GetAllMemoryDatabase();

    std::shared_ptr<FullDb::MemScopeDatabase> GetMemScopeDatabase(const std::string &fileId);
    std::shared_ptr<FullDb::MemSnapshotDatabase> GetMemSnapshotDatabase(const std::string &fileId);
    std::vector<FullDb::MemScopeDatabase *> GetAllMemScopeDatabase();
    std::vector<FullDb::MemSnapshotDatabase *> GetAllMemSnapshotDatabase();
    std::shared_ptr<Summary::VirtualSummaryDataBase> GetSummaryDatabaseByRankId(const std::string &rankId);
    std::shared_ptr<Summary::VirtualSummaryDataBase> GetSummaryDatabaseWithCluster(
        const std::string &cluster, const std::string &rankId);
    std::shared_ptr<Summary::VirtualSummaryDataBase> GetSummaryDataBaseByFileId(const std::string &fileId);
    std::shared_ptr<Summary::VirtualSummaryDataBase> CreateSummaryDatabase(
        const std::string &rankId, const std::string &dbPath);
    std::vector<Summary::VirtualSummaryDataBase *> GetAllSummaryDatabase();

    std::string GetDbPathByRankId(const std::string &rankId);
    std::shared_ptr<VirtualTraceDatabase> GetTraceDatabaseWithOutHost(const std::string &rankId);
    std::shared_ptr<VirtualTraceDatabase> GetTraceDatabaseInCluster(
        const std::string &clusterPath, const std::string &rankId);
    DataType GetDataType(const std::string &fileId);
    DataType GetDataTypeByRank(const std::string &rankId);
    void SetDataType(DataType type, const std::string &fileId);
    FileType GetFileType(const std::string &fileId);
    FileType GetFileTypeByRankId(const std::string &rankId);
    void SetFileType(FileType type, const std::string &fileId);
    bool ResetBaseline(bool force);
    void SetDbPathMapping(const std::string &rankId, const std::string &dbPath, const std::string &hostId);
    bool IsContainDatabasePath(const std::string &databasePath);
    std::string GetDeviceIdFromRankId(const std::string &rankId);
    inline std::vector<std::string> GetDbPathByHost(const std::string &id) {
        if (host2DbPath.find(id) != host2DbPath.end()) {
            return host2DbPath[id];
        }
        return {};
    }
    std::string GetRankIdByFileId(const std::string &fileId);

    std::string GetAnyTraceDatabaseId();

    std::string GetFileIdByRankId(const std::string &rankId) const;

    void UpdateRankIdToDeviceId(const std::string &fileId, const std::string &rankId, const std::string &deviceId);
    void SetRankIdFileIdMapping(const std::string &rankId, const std::string &fileId);

  private:
    using RankId = std::string;
    using FileId = std::string;
    using DbPath = FileId;
    using HostId = std::string;
    using ClusterProjectPath = std::string;
    using ClusterDbPath = std::string;
    DataBaseManager() = default;
    ~DataBaseManager() = default;
    std::recursive_mutex mutex;
    std::unordered_map<FileId, DataType> dataTypeMap;
    std::unordered_map<FileId, FileType> fileTypeMap;

    std::map<std::string, std::recursive_mutex> dbMutexMap;
    std::map<RankId, DbPath> dbFilePathMap;
    std::map<RankId, FileId> rankId2FileIdMap;
    std::map<FileId, RankId> fileIdToRankIdMap;
    std::map<ClusterProjectPath, ClusterDbPath> clusterProject2DbPathMap;
    std::map<HostId, std::vector<DbPath>> host2DbPath;
    std::unordered_set<std::string> databasePathSet;
    std::map<FileId, std::shared_ptr<DBConnectionPool<VirtualTraceDatabase>>> traceDatabaseMap;
    std::map<ClusterDbPath, std::shared_ptr<DBConnectionPool<VirtualClusterDatabase>>> clusterDatabaseMap;
    std::map<RankId, std::shared_ptr<Memory::VirtualMemoryDataBase>> memoryDatabaseMap;
    std::map<FileId, std::shared_ptr<FullDb::MemScopeDatabase>> memScopeDatabaseMap;
    std::map<FileId, std::shared_ptr<FullDb::MemSnapshotDatabase>> memSnapshotDatabaseMap;
    std::map<RankId, std::shared_ptr<Summary::VirtualSummaryDataBase>> summaryDatabaseMap;

    std::map<RankId, std::shared_ptr<Memory::VirtualMemoryDataBase>> memoryBaselineDatabaseMap;
    std::map<RankId, std::shared_ptr<Summary::VirtualSummaryDataBase>> summaryBaselineDatabaseMap;

    std::map<std::string, std::string> rankIdToDeviceIdMap; // key: fileId + rankId , value: deviceId
    std::recursive_mutex &GetDbMutex(const std::string &fileId);
    void SetClusterProjectDbPathMapping(const std::string &projectPath, const std::string &dbPath);
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_DATABASEMANAGER_H
