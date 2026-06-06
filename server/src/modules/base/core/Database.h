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

#ifndef DATA_INSIGHT_CORE_MODULE_CORE_DATABASE_BASE_H
#define DATA_INSIGHT_CORE_MODULE_CORE_DATABASE_BASE_H

#include <vector>
#include <string>
#include <mutex>
#include "ServerLog.h"
#include "sqlite3.h"
#include "BaseDomain.h"
#include "TryOpt.h"
#include "SqlitePreparedStatement.h"

namespace Dic {
namespace Module {
class DatabaseException : public std::exception {
  public:
    explicit DatabaseException(const char *message) : message(message) {};
    DatabaseException(const char *message, bool isError) : isError(isError), message(message) {};
    const char *What() const { return message; };
    void Log(std::string prefix) const {
        if (isError) {
            Server::ServerLog::Error(prefix, message);
        } else {
            Server::ServerLog::Warn(prefix, message);
        }
    }

  private:
    bool isError = true;
    const char *message;
};
class Database {
  public:
    explicit Database(std::recursive_mutex &sqlMutex) : mutex(sqlMutex) {};
    virtual ~Database();
    virtual bool CreateDbIfNotExist(const std::string &dbPath);
    virtual bool OpenDb(const std::string &dbPath, bool clearAllTable);
    virtual bool AttachDb(const std::string &dbPath);
    virtual bool IsOpen() const;
    void CloseDb();
    virtual bool StartTransaction();
    virtual bool RollbackTransaction();
    virtual bool EndTransaction();
    virtual std::string GetDbPath();
    virtual void SetDbPath(const std::string &dbPath);
    virtual bool GetTableList(std::vector<std::string> &tableList) const;
    virtual std::unique_ptr<SqlitePreparedStatement> CreatPreparedStatement(const std::string &sql);
    virtual std::unique_ptr<SqlitePreparedStatement> CreatPreparedStatement();
    bool ExecSql(const std::string &sql) const;
    bool DropSomeTables(const std::vector<std::string> &tableNames) const;
    bool DropAllTable();
    bool IsDatabaseVersionChange() const;
    bool QueryMetaVersion();
    std::string GetMetaVersion() const;
    bool SetDataBaseVersion(const std::string &targetVersion = ""); // targetVersion仅作为内部方法使用，请勿引入外部输入
    std::string QueryValueFromMetaDataByName(const std::string &name);

    bool CheckTableExist(const std::string &tableName);
    bool CheckTablesExist(const std::vector<std::string> &tablesName);
    bool CheckColumnExist(const std::string &tableName, const std::string &columnName);
    bool CheckStringInColumn(
        const std::string &tableName, const std::string &columnName, const std::string &searchString);
    bool ExtendColumns(const std::string &tableName, const std::vector<std::string> &columns);
    bool CreateMetaDataTableForText();
    bool UpdateMetaDataTable(const std::string &name, const std::string &value);
    bool UpdateMetaDataTableWithNoPrimaryKey(const std::string &name, const std::string &value);
    virtual std::vector<ColumnAtt> QueryTableInfoByName(const std::string &tableName);
    virtual uint64_t QueryCountByTableName(const PageQuery &query, const std::vector<ColumnAtt> &columns);
    virtual std::vector<std::map<std::string, std::string>> QueryDataByPage(
        const PageQuery &query, const std::vector<ColumnAtt> &columns);
    std::vector<LinkInfo> QueryTableNameAndCol(const std::string &linkName);

    /**
     * 查询所有符合前缀的表名
     * @param prefix 表名前缀
     * @return 所有符合前缀的表名
     */
    std::vector<std::string> QueryTableNamesByPrefix(const std::string &prefix);
    bool CheckValueFromStatusInfoTable(const std::string &key, const std::string &refValue);
    bool UpdateValueIntoStatusInfoTable(const std::string &key, const std::string &value);
    std::unordered_map<std::string, std::string> QueryTranslate(bool isZh);
    std::string QueryDatabaseVersion() const;
    static std::string GetCompileDataBaseVersion();

    /**
     * @brief 设置属性
     * @param key 属性键
     * @param value 属性值
     */
    void SetAttr(const std::string &key, const std::string &value);

    /**
     * @brief 获取属性
     * @param key 属性键
     * @param defaultValue 默认值（当属性不存在时返回）
     * @return 属性值或默认值
     */
    std::string GetAttr(const std::string &key, const std::string &defaultValue = "") const;

    /**
     * @brief 检查属性是否存在
     * @param key 属性键
     * @return true-存在，false-不存在
     */
    bool HasAttr(const std::string &key) const;

    /**
     * @brief 移除属性
     * @param key 属性键
     */
    void RemoveAttr(const std::string &key);

    /**
     * @brief 获取所有属性
     * @return 属性集合的常量引用
     */
    const std::map<std::string, std::string> &GetAllAttrs() const;

    /**
     * @brief 批量设置属性
     * @param attrsMap 属性键值对集合
     */
    void SetAttrs(const std::map<std::string, std::string> &attrsMap);

  protected:
    bool CheckTableContainData(const std::string &tableName);
    virtual bool SetConfig();
    static std::string CheckSqlString(const std::string &src);
    static std::string sqlite3_column_string(sqlite3_stmt *stmt, int iCol);
    void FastGetString(sqlite3_stmt *stmt, int iCol, std::string &output);
    static std::string Sqlite3ColumnConvertStr(int colType, sqlite3_stmt *stmt, int iCol);
    std::string Sqlite3ColumnConvertStrReturnNull(int colType, sqlite3_stmt *stmt, int iCol);
    std::string GetLastError();
    sqlite3 *db = nullptr;
    std::recursive_mutex &mutex;
    bool isOpen = false;
    std::string path;
    const int bindStartIndex = 1;
    const int resultStartIndex = 0;
    const int timeoutMs = 50000;
    const std::string infoTable = "status_info";
    bool isLowCamel = false;
    std::string metaVersion;
    const std::string metaDataTable = "META_DATA";
    std::unordered_map<std::string, std::string> rankToDeviceMap;

    /**
     * @brief 数据库属性集合，用于存储数据库类型、子类型、来源等特征信息
     */
    std::map<std::string, std::string> attrs;

    /**
     * @brief attrs 属性的互斥锁，用于保证线程安全
     */
    mutable std::recursive_mutex attrsMutex;

    std::string GetValueFromMetaDataTable(const std::string &name);
    bool CreateStatusInfoTable(); // 创建表时未加锁，需要在调用处加锁
    std::string GetValueFromStatusInfoTable(const std::string &key);
    bool CheckAndResetDatabaseOnVersionChange();
    template <typename... Args>
    static inline std::unique_ptr<SqliteResultSet> ExecuteQuery(
        std::unique_ptr<SqlitePreparedStatement> &stmt, const std::string &sql, Args &&...args) {
        if (stmt == nullptr) {
            throw DatabaseException("Failed to prepare sql.");
        }
        if (!stmt->Prepare(sql)) {
            throw DatabaseException("Failed to prepare sql.");
        }
        stmt->Reset();
        stmt->BindParams(std::forward<Args>(args)...);
        auto result = stmt->ExecuteQuery();
        if (result == nullptr) {
            throw DatabaseException("Failed to ExecuteQuery.");
        }
        return result;
    };

    static std::string ComputeDataPageSql(const PageQuery &query, std::vector<std::string> &columnName);

    static std::string ComputeConditionSql(const PageQuery &query, std::vector<std::string> &columnName);

    // 通用表格查询SQL构建和参数绑定方法
    static std::string BuildQueryFiltersConditionSql(const std::map<std::string, std::string> &filters);
    static std::string BuildQueryRangeFiltersConditionSql(
        const std::map<std::string, std::pair<double, double>> &rangeFilters);
    static std::string BuildQueryOrderSql(const std::string &orderBy, bool desc);
    static void CommonBindFiltersParams(
        const std::map<std::string, std::string> &filters, sqlite3_stmt *stmt, int &bindIdx);
    static void CommonBindRangeFiltersParams(
        const std::map<std::string, std::pair<double, double>> &rangeFilters, sqlite3_stmt *stmt, int &bindIdx);
    static void CommonBindPaginationParams(int64_t pageSize, int64_t currentPage, sqlite3_stmt *stmt, int &bindIdx);

  private:
    size_t maxDbFileSize = 50ULL * 1024 * 1024 * 1024;
};
} // end of namespace Module
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_MODULE_CORE_DATABASE_BASE_H
