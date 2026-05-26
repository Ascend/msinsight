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

#ifndef PROFILER_SERVER_SUMMARY_DATABASE_H
#define PROFILER_SERVER_SUMMARY_DATABASE_H

#include "Database.h"
#include "ProtocolMessage.h"
#include "SummaryDef.h"
#include "SummaryProtocolRequest.h"
#include "SummaryProtocolResponse.h"
#include "OperatorProtocolRequest.h"
#include "OperatorProtocolResponse.h"
#include "TableDefs.h"
#include "NumDefs.h"
#include "CommonDefs.h"
#include "ClusterDef.h"

namespace Dic::Module::Summary {
using namespace Dic::Server;

class VirtualSummaryDataBase : public Database {
  public:
    bool levelIsL0 = true;

    explicit VirtualSummaryDataBase(std::recursive_mutex &sqlMutex) : Database(sqlMutex) {};
    ~VirtualSummaryDataBase() override = default;

    virtual bool QueryComputeOpDetail(
        Protocol::ComputeDetailParams params, std::vector<Protocol::ComputeDetail> &computeDetails) = 0;
    virtual bool QueryTotalNumByAcceleratorCore(std::string name, int64_t &totalNum) = 0;

    virtual bool QueryCommunicationOpDetail(
        Protocol::CommunicationDetailParams params, std::vector<Protocol::CommunicationDetail> &computeDetails) = 0;

    virtual bool QueryOperatorDurationInfo(Protocol::OperatorDurationReqParams &reqParams, Protocol::QueryType type,
        std::vector<Protocol::OperatorDurationRes> &data) = 0;

    virtual bool QueryOperatorStatisticInfo(
        Protocol::OperatorStatisticReqParams &reqParams, Protocol::OperatorStatisticInfoResponse &response) = 0;

    virtual bool QueryOperatorDetailInfo(
        Protocol::OperatorStatisticReqParams &reqParams, Protocol::OperatorDetailInfoResponse &response) = 0;
    virtual bool QueryAllOperatorDetailInfo(Protocol::OperatorStatisticReqParams &reqParams,
        std::vector<Protocol::OperatorDetailInfoRes> &res, std::string &level) = 0;

    virtual bool QueryOperatorMoreInfo(
        Protocol::OperatorMoreInfoReqParams &reqParams, Protocol::OperatorMoreInfoResponse &response) = 0;
    virtual bool QueryAllOperatorStatisticInfo(
        Protocol::OperatorStatisticReqParams &reqParams, std::vector<Protocol::OperatorStatisticInfoRes> &res) = 0;

    virtual bool QueryBandwidthContentionMatMulData(std::vector<BandwidthContentionMatMulInfo> &res) = 0;
    bool ExecuteQueryBandwidthContentionMatMulData(std::vector<BandwidthContentionMatMulInfo> &res, std::string &sql) {
        sqlite3_stmt *stmt = nullptr;
        int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (result != SQLITE_OK) {
            ServerLog::Error("Failed to query bandwidth contention matmul data. Error: ", sqlite3_errmsg(db));
            return false;
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int col = resultStartIndex;
            BandwidthContentionMatMulInfo info;
            info.name = sqlite3_column_string(stmt, col++);
            info.startTime = sqlite3_column_double(stmt, col++);
            info.duration = sqlite3_column_double(stmt, col++);
            res.emplace_back(info);
        }
        sqlite3_finalize(stmt);
        return true;
    }

    uint64_t QueryMinStartTime();
    static inline std::string GetFileIdFromCombinationId(const std::string &str) {
        auto len = MSPROF_PREFIX.length();
        if (str.length() <= len || str.compare(0, len, MSPROF_PREFIX) != 0) {
            return str;
        }

        auto index = str.find_last_of(MSPROF_CONNECT);
        if (index == std::string::npos) {
            return str;
        }

        return str.substr(len, index - len - 1);
    }

    static inline std::string GetDeviceIdFromCombinationId(const std::string &str) {
        auto len = MSPROF_PREFIX.length();
        if (str.length() <= len || str.compare(0, len, MSPROF_PREFIX) != 0) {
            return str;
        }

        auto index = str.find_last_of(MSPROF_CONNECT);
        if (index == std::string::npos) {
            return str;
        }

        return str.substr(index + MSPROF_CONNECT.length() - 1);
    }

    std::string OperatorGetLevel(const std::vector<Protocol::OperatorDetailInfoRes> &res) {
        std::string level;
        if (res.empty()) {
            level = levelIsL0 ? "l0" : "l1";
        } else if (res.at(0).inputShape.empty()) {
            level = "l0";
            levelIsL0 = true;
        } else {
            level = "l1";
            levelIsL0 = false;
        }
        return level;
    }

    std::string JoinExtraColName(const std::vector<std::string> &cols) {
        std::string pmuColumnNames;
        if (!cols.empty()) {
            pmuColumnNames = ",";
            pmuColumnNames += StringUtil::join(cols, ',');
        }
        return pmuColumnNames;
    }

    const std::set<std::string> &GetPmuColumns() const { return pmuColumns_; }
    // kernelparser解析的时候赋值
    std::set<std::string> pmuColumns_;
};
}

#endif // PROFILER_SERVER_SUMMARY_DATABASE_H
