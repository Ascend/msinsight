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

#include "HostInfoTable.h"
namespace Dic::Module::Timeline {
std::string HostInfoTable::GetHost(const std::string &fileId) {
    std::string host;
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(fileId);
    if (database == nullptr) {
        ClearThreadLocal();
        return host;
    }
    host = database->QueryHostInfo();
    return host;
}

void HostInfoTable::SetHostUid(HostInfoPO &hostInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    hostInfoPO.hostUid = resultSet->GetString(HostInfoColumn::HOST_UID);
}

void HostInfoTable::SetHostName(HostInfoPO &hostInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    hostInfoPO.hostName = resultSet->GetString(HostInfoColumn::HOST_NAME);
}
}
