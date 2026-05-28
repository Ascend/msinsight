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

#ifndef PROFILER_SERVER_SESSIONTIMEINFOTABLE_H
#define PROFILER_SERVER_SESSIONTIMEINFOTABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct HostInfoPO {
    std::string hostUid;
    std::string hostName;
};
class HostInfoTable : public Table<HostInfoPO> {
  public:
    HostInfoTable() = default;
    ~HostInfoTable() override = default;
    std::string GetHost(const std::string &fileId);

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {HostInfoColumn::HOST_UID, SetHostUid},
            {HostInfoColumn::HOST_NAME, SetHostName},
        };
        return assignMap;
    }

    const std::string &GetTableName() override {
        static std::string tableName = "HOST_INFO";
        return tableName;
    }
    static void SetHostUid(HostInfoPO &hostInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetHostName(HostInfoPO &hostInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_SESSIONTIMEINFOTABLE_H
