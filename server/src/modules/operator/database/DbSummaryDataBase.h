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

#ifndef PROFILER_SERVER_DBSUMMARYDATABASE_H
#define PROFILER_SERVER_DBSUMMARYDATABASE_H

#include "VirtualSummaryDataBase.h"
#include "OperatorProtocolRequest.h"
#include "OperatorGroupConverter.h"

namespace Dic::Module::FullDb {
using namespace Dic::Protocol;
class DbSummaryDataBase : public Summary::VirtualSummaryDataBase {
  public:
    explicit DbSummaryDataBase(std::recursive_mutex &sqlMutex) : Summary::VirtualSummaryDataBase(sqlMutex) {};
    virtual ~DbSummaryDataBase() {};

    bool OpenDb(const std::string &dbPath, bool clearAllTable) override;
    bool QueryComputeOpDetail(
        Protocol::ComputeDetailParams params, std::vector<Protocol::ComputeDetail> &computeDetails) override;
    bool QueryTotalNumByAcceleratorCore(std::string name, int64_t &totalNum) override;
    bool QueryCommunicationOpDetail(
        Protocol::CommunicationDetailParams params, std::vector<Protocol::CommunicationDetail> &commDetails) override;
    bool QueryOperatorDurationInfo(Protocol::OperatorDurationReqParams &reqParams, Protocol::QueryType type,
        std::vector<Protocol::OperatorDurationRes> &data) override;
    bool QueryOperatorStatisticInfo(
        Protocol::OperatorStatisticReqParams &reqParams, Protocol::OperatorStatisticInfoResponse &response) override;
    bool QueryAllOperatorStatisticInfo(
        OperatorStatisticReqParams &reqParams, std::vector<Protocol::OperatorStatisticInfoRes> &res) override;
    bool QueryOperatorDetailInfo(
        Protocol::OperatorStatisticReqParams &reqParams, Protocol::OperatorDetailInfoResponse &response) override;
    bool QueryAllOperatorDetailInfo(Protocol::OperatorStatisticReqParams &reqParams,
        std::vector<Protocol::OperatorDetailInfoRes> &res, std::string &level) override;
    bool QueryOperatorMoreInfo(
        Protocol::OperatorMoreInfoReqParams &reqParams, Protocol::OperatorMoreInfoResponse &response) override;
    static void ParserEnd(const std::string &rankId, const std::string &fileId, bool result, const std::string &msg);
    static void Reset();

    bool QueryBandwidthContentionMatMulData(std::vector<BandwidthContentionMatMulInfo> &res) override;

  private:
    std::set<std::string> FetchPmuColumnNames();
    std::string GenerateQueryDetailSqlForOperator();
    std::string CreatPMUTmpTableSql(const std::set<std::string> &cols);
    std::string GetPMUTmpTableColSql(const std::set<std::string> &cols);
    bool QueryStatisticTotalNum(Protocol::OperatorStatisticReqParams &reqParams, int64_t &total);
    std::string GenerateQueryStatisticSql(Protocol::OperatorStatisticReqParams &reqParams);
    std::string GenStatSqlWithCommunication();
    std::string GenStatSqlWithDeviceId(const std::string group);
    std::string GenStatSql(const std::string group);
    std::string GenComputeSql(const Protocol::ComputeDetailParams &request);
    static std::string GenerateQueryCategoryDurationSql(OperatorDurationReqParams &reqParams);
    static std::string GenerateQueryComputeUnitDurationSql(OperatorDurationReqParams &reqParams);
    bool QueryDetailTotalNum(OperatorStatisticReqParams &reqParams, int64_t &total);
    std::string GenerateAllQueryDetailSql(OperatorStatisticReqParams &reqParams);
    std::string GenerateQueryDetailSql(OperatorStatisticReqParams &reqParams);
    bool QueryMoreInfoTotalNum(OperatorMoreInfoReqParams &reqParams, int64_t &total);
    std::string GenerateQueryMoreInfoSql(OperatorMoreInfoReqParams &reqParams);
    std::string GetCommSql(const CommunicationDetailParams &request);
    bool CheckOperatorTableExist(const std::string &group, const std::string &logInfo);

    const uint32_t maxCategorySize = 50;

    void BindSqliteParam(sqlite3_stmt *stmt, OperatorMoreInfoReqParams &reqParams);
    static std::string GenerateQueryCategoryDurationSqlForHCCL(
        const Dic::Protocol::OperatorGroupConverter::OperatorGroup &operatorGroup);
    std::string &GenerateQueryMoreInfoSqlForHCCL(std::string &sql) const;
    std::string &GenerateQueryMoreInfoSqlForOther(std::string &sql);
    std::string &GenerateQueryDetailSqlForHCCL(std::string &sql) const;
    void GenerateMoreInfoTotalNumForOther(std::string &sql, OperatorGroupConverter::OperatorGroup opGroup) const;
    static void GenerateRangeQueryFiltersSql(
        std::vector<std::pair<std::string, std::vector<std::string>>> &rangeFilters, std::string &sql);
    std::vector<std::pair<std::string, std::vector<std::string>>> ConvertFiltersToRangeFilters(
        std::vector<std::pair<std::string, std::string>> &filters);
    template <typename T> void GenerateQueryFiltersSql(T &reqParams, std::string &sql);
    template <typename T> void BindQueryFilters(T &reqParams, sqlite3_stmt *stmt, int &index);
    static void BindIdList(const std::vector<std::pair<std::string, std::vector<std::string>>> &rangeFilters,
        sqlite3_stmt *stmt, int &index);
    bool GenerateQueryMoreInfoFilters(OperatorMoreInfoReqParams &reqParams, std::string &sql);
    bool ExecSqlGetDetailInfo(std::string sql, Protocol::OperatorStatisticReqParams &reqParams,
        std::vector<Protocol::OperatorDetailInfoRes> &res);
    bool ExecSqlGetStatisticInfo(std::string sql, Protocol::OperatorStatisticReqParams &reqParams,
        std::vector<Protocol::OperatorStatisticInfoRes> &res);
    bool AddCommunicationOpTableOpTypeIfNotExists();
    OperatorDetailInfoRes GetOperatorDetailRow(sqlite3_stmt *stmt);
    std::string GetGroupNameByIdListStr(const std::string &idListStr);

    std::string blockNumColumnName;
};

}
#endif // PROFILER_SERVER_DBSUMMARYDATABASE_H
