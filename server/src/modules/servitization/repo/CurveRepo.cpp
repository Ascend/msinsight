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
#include "TryOpt.h"
#include "ServerLog.h"
#include "StringUtil.h"
#include "CurveRepo.h"
namespace Dic::Module::IE {
std::vector<std::string> CurveRepo::QueryAllViews(const std::string &fileId) {
    auto dataBase = context->GetDatabase(fileId);
    if (!TryOpt(dataBase, "Query all views get connection failed!")) {
        return {};
    }
    const std::string sql = "SELECT name FROM sqlite_master WHERE type = 'view';";
    auto stmt = dataBase->CreatPreparedStatement(sql);
    if (!TryOpt(stmt, "Query all views prepare sql failed!")) {
        return {};
    }
    auto result = stmt->ExecuteQuery();
    if (!TryOpt(result, "Query all views get result failed!")) {
        return {};
    }
    std::vector<std::string> res;
    while (result->Next()) {
        std::string viewName = result->GetString("name");
        if (StringUtil::EndWith(viewName, "_curve")) {
            res.emplace_back(viewName);
        }
    }
    return res;
}

std::vector<ColumnAtt> CurveRepo::QueryTableInfoByName(const std::string &fileId, const std::string &tableName) {
    auto dataBase = context->GetDatabase(fileId);
    if (!TryOpt(dataBase, "Query table info by name get connection failed!")) {
        return {};
    }
    return dataBase->QueryTableInfoByName(tableName);
}

std::vector<std::map<std::string, std::string>> CurveRepo::QueryDataByColumn(
    const std::string &fileId, const std::string &tableName, const std::vector<ColumnAtt> &columns) {
    if (!StringUtil::CheckSqlValid(tableName) || columns.empty()) {
        return {};
    }
    std::vector<std::string> columnName;
    for (const auto &item : columns) {
        columnName.emplace_back("\"" + item.key + "\"");
    }
    const std::string columnNames = StringUtil::join(columnName, ",");
    std::string sql = "SELECT " + columnNames + " FROM '" + tableName + "' ORDER BY " + columnName.front();
    auto dataBase = context->GetDatabase(fileId);
    if (!TryOpt(dataBase, "Query data by column get connection failed!")) {
        return {};
    }
    auto stmt = dataBase->CreatPreparedStatement(sql);
    if (!TryOpt(stmt, "Query data by column prepare sql failed!")) {
        return {};
    }
    auto result = stmt->ExecuteQuery();
    if (!TryOpt(result, "Query data by column get result failed!")) {
        return {};
    }
    std::vector<std::map<std::string, std::string>> res;
    while (result->Next()) {
        std::map<std::string, std::string> data;
        for (const auto &item : columns) {
            data[item.key] = result->GetString(item.key);
            if (std::empty(data[item.key])) {
                data[item.key] = "0";
            }
        }
        res.emplace_back(data);
    }
    return res;
}

std::vector<std::map<std::string, std::string>> CurveRepo::QueryDataByColumnPage(
    const PageQuery &query, const std::vector<ColumnAtt> &columns) {
    std::vector<std::string> columnName = ComputeColNames(query, columns);
    if (std::empty(columnName)) {
        return {};
    }
    if (!query.orderBy.empty() &&
        std::find(columnName.begin(), columnName.end(), "\"" + query.orderBy + "\"") == columnName.end()) {
        return {};
    }
    const std::string conditionName = columns[0].key;
    const std::string columnNames = StringUtil::join(columnName, ",");
    std::string sql = "SELECT " + columnNames + " FROM '" + query.viewName + "' WHERE 1=1 ";
    bool rangeIsValid = !query.start.empty() && !query.end.empty();
    bool rangeIsInteger =
        rangeIsValid && StringUtil::CheckSqlValid(query.start) && StringUtil::CheckSqlValid(query.end);
    // 专门处理整数
    if (rangeIsValid && rangeIsInteger) {
        sql += " AND " + conditionName + " >= " + query.start + " AND " + conditionName + " <= " + query.end;
    }
    if (rangeIsValid && !rangeIsInteger) {
        sql += " AND " + conditionName + " >= ? AND " + conditionName + " <= ? ";
    }
    sql = AppendConditionSql(query, sql);
    auto dataBase = context->GetDatabase(query.fileId);
    if (!TryOpt(dataBase, "Query data by column page get connection failed!")) {
        return {};
    }
    auto stmt = dataBase->CreatPreparedStatement(sql);
    if (!TryOpt(stmt, "Query data by column page prepare sql failed!")) {
        return {};
    }
    if (rangeIsValid && !rangeIsInteger) {
        stmt->BindParams(query.start, query.end);
    }
    auto result = stmt->ExecuteQuery();
    if (!TryOpt(result, "Query data by column page get result failed!")) {
        return {};
    }
    std::vector<std::map<std::string, std::string>> res;
    while (result->Next()) {
        std::map<std::string, std::string> data;
        for (const auto &item : columns) {
            data[item.key] = result->GetString(item.key);
            if (std::empty(data[item.key])) {
                data[item.key] = "0";
            }
        }
        res.emplace_back(data);
    }
    return res;
}

std::string &CurveRepo::AppendConditionSql(const PageQuery &query, std::string &sql) const {
    std::string orderBySql;
    if (!std::empty(query.order) && !std::empty(query.orderBy)) {
        orderBySql = query.order == "descend" ? " ORDER BY \"" + query.orderBy + "\" DESC "
                                              : " ORDER BY \"" + query.orderBy + "\" ASC ";
    }
    sql += orderBySql;
    const std::string limitSql =
        " LIMIT " + std::to_string(query.size) + " OFFSET " + std::to_string(query.ComputeOffset());
    sql += limitSql;
    return sql;
}

std::vector<std::string> CurveRepo::ComputeColNames(
    const PageQuery &query, const std::vector<ColumnAtt> &columns) const {
    if (columns.empty()) {
        return {};
    }
    std::vector<std::string> columnName;
    for (const auto &item : columns) {
        columnName.emplace_back("\"" + item.key + "\"");
    }
    if (!StringUtil::CheckSqlValid(query.viewName)) {
        return {};
    }
    return columnName;
}

uint64_t CurveRepo::QueryCountByTableName(const PageQuery &query, const std::string &abscissa) {
    if (!StringUtil::CheckSqlValid(query.viewName)) {
        return 0;
    }
    std::string sql = "SELECT COUNT(*) as count FROM '" + query.viewName + "' WHERE 1 = 1 ";
    bool rangeIsValid = !query.start.empty() && !query.end.empty();
    bool rangeIsInteger =
        rangeIsValid && StringUtil::CheckSqlValid(query.start) && StringUtil::CheckSqlValid(query.end);
    if (rangeIsValid && rangeIsInteger) {
        sql += " AND " + abscissa + " >= " + query.start + " AND " + abscissa + " <= " + query.end;
    }
    if (rangeIsValid && !rangeIsInteger) {
        sql += " AND " + abscissa + " >= ? AND " + abscissa + " <= ? ";
    }
    auto dataBase = context->GetDatabase(query.fileId);
    if (!TryOpt(dataBase, "Query count by table name get connection failed!")) {
        return {};
    }
    auto stmt = dataBase->CreatPreparedStatement(sql);
    if (!TryOpt(stmt, "Query count by table name prepare sql failed!")) {
        return {};
    }
    if (rangeIsValid && !rangeIsInteger) {
        stmt->BindParams(query.start, query.end);
    }
    auto result = stmt->ExecuteQuery();
    if (!TryOpt(result, "Query count by table name get result failed!")) {
        return {};
    }
    if (result->Next()) {
        return result->GetUint64("count");
    }
    return 0;
}

std::string CurveRepo::QueryTableNameDesc(const std::string &fileId, const std::string &tableName, bool isZh) {
    auto dataBase = context->GetDatabase(fileId);
    if (!TryOpt(dataBase, "Query table name desc get connection failed!")) {
        return {};
    }
    auto translate = dataBase->QueryTranslate(isZh);
    return translate[tableName];
}
} // namespace Dic::Module::IE
