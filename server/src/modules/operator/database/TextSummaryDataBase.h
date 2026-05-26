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

#ifndef PROFILER_SERVER_COMMUNICATION_DATABASE_H
#define PROFILER_SERVER_COMMUNICATION_DATABASE_H
#include <set>
#include "VirtualSummaryDataBase.h"
#include "OperatorGroupConverter.h"

namespace Dic {
namespace Module {
namespace Summary {
using namespace Dic::Protocol;
class TextSummaryDataBase : public VirtualSummaryDataBase {
  public:
    explicit TextSummaryDataBase(std::recursive_mutex &sqlMutex);
    ~TextSummaryDataBase() override;

    bool SetConfig() override;
    bool OpenDb(const std::string &dbPath, bool clearAllTable) override;
    bool CreateTable();
    bool DropTable();
    bool InitStmt(const std::vector<std::string> &columns);
    void ReleaseStmt();

    void InsertKernelDetail(const Kernel &kernel, const std::vector<std::string> &columns);
    void SaveKernelDetail(const std::vector<std::string> &columns);
    uint64_t QueryMinStartTime();
    std::set<std::string> QueryRankIds();

    bool QueryComputeOpDetail(ComputeDetailParams params, std::vector<ComputeDetail> &computeDetails) override;
    bool QueryTotalNumByAcceleratorCore(std::string name, int64_t &totalNum) override;

    bool QueryCommunicationOpDetail(
        CommunicationDetailParams params, std::vector<CommunicationDetail> &computeDetails) override;

    bool QueryOperatorDurationInfo(
        OperatorDurationReqParams &reqParams, QueryType type, std::vector<OperatorDurationRes> &data) override;

    bool QueryOperatorStatisticInfo(
        OperatorStatisticReqParams &reqParams, OperatorStatisticInfoResponse &response) override;

    bool QueryOperatorDetailInfo(OperatorStatisticReqParams &reqParams, OperatorDetailInfoResponse &response) override;

    bool QueryOperatorMoreInfo(OperatorMoreInfoReqParams &reqParams, OperatorMoreInfoResponse &response) override;

    bool UpdateParseStatus(const std::string &status);
    bool HasFinishedParseLastTime();
    bool QueryAllOperatorStatisticInfo(
        OperatorStatisticReqParams &reqParams, std::vector<Protocol::OperatorStatisticInfoRes> &res) override;

    bool QueryBandwidthContentionMatMulData(std::vector<BandwidthContentionMatMulInfo> &res) override;

  private:
    const std::string kernelParseState = "Kernel files parsing status";
    bool hasInitStmt = false;
    sqlite3_stmt *insertKernelStmt = nullptr;
    const uint32_t cacheSize = 600;
    const uint32_t maxCategorySize = 50;
    std::vector<Kernel> kernelCache;

    sqlite3_stmt *GetKernelStmt(uint64_t paramLen, const std::vector<std::string> &columns);
    void InsertKernelDetailList(const std::vector<Kernel> &kernelVec, const std::vector<std::string> &columns);
    std::string GenSortSql(std::string orderBy, std::string order);
    std::string GenComputeSql(Protocol::ComputeDetailParams request);
    std::string GetCommSql(Protocol::CommunicationDetailParams request);
    std::string GetAcceleratorCoreSql(const bool isCommunication);

    std::string GenerateQueryCategoryDurationSql(OperatorDurationReqParams &reqParams);

    std::string GenerateQueryComputeUnitDurationSql(OperatorDurationReqParams &reqParams);

    std::string GenerateQueryStatisticSql(OperatorStatisticReqParams &reqParams);
    std::string GetQueryBaseStaticSql(Protocol::OperatorStatisticReqParams &reqParams);
    bool ExecSqlGetStaticInfo(const std::string &sql, Protocol::OperatorStatisticReqParams &reqParams,
        std::vector<Protocol::OperatorStatisticInfoRes> &res);

    std::string GenerateQueryDetailSql(OperatorStatisticReqParams &reqParams);

    std::string GenerateQueryMoreInfoSql(OperatorMoreInfoReqParams &reqParams);

    bool QueryStatisticTotalNum(OperatorStatisticReqParams &reqParams, int64_t &total);

    bool QueryDetailTotalNum(OperatorStatisticReqParams &reqParams, int64_t &total);

    bool QueryMoreInfoTotalNum(OperatorMoreInfoReqParams &reqParams, int64_t &total);

    void BindSqliteParam(sqlite3_stmt *stmt, Protocol::OperatorMoreInfoReqParams &reqParams);

    template <typename T> void GenerateQueryFiltersSql(T &reqParams, std::string &sql);
    template <typename T> void BindQueryFilters(T &reqParams, sqlite3_stmt *stmt, int &index);

    bool IsOperatorGroupInType(OperatorGroupConverter::OperatorGroup operatorGroup);
    std::set<std::string> FetchPmuColumnNames();
    std::string GetQueryDetailBaseSql(Protocol::OperatorStatisticReqParams &reqParams, bool isLimit);
    std::string GetQuerySqlNofilter(Protocol::OperatorStatisticReqParams &reqParams, const bool isCommunication,
        const std::string &group, const std::string &name);
    bool ExecSqlGetDetailInfo(std::string sql, Protocol::OperatorStatisticReqParams &reqParams,
        std::vector<Protocol::OperatorDetailInfoRes> &res, std::string &level);
    bool ExecSqlGetRes(sqlite3_stmt *stmt, std::vector<Protocol::OperatorDetailInfoRes> &res);
    std::vector<Protocol::OperatorDetailInfoRes> ExecSqlGetMoreInfo(sqlite3_stmt *stmt);
    bool QueryAllOperatorDetailInfo(Protocol::OperatorStatisticReqParams &reqParams,
        std::vector<Protocol::OperatorDetailInfoRes> &res, std::string &level) override;
};

} // end of namespace Summary
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_SUMMARY_DATABASE_H
