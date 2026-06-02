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

#ifndef PROFILER_SERVER_JSON_CLUSTER_DATABASE_H
#define PROFILER_SERVER_JSON_CLUSTER_DATABASE_H

#include <set>
#include "VirtualClusterDatabase.h"
#include "ClusterDef.h"
#include "ProtocolMessage.h"
#include "SummaryProtocolResponse.h"
#include "TimelineProtocolResponse.h"
#include "SummaryProtocolRequest.h"
#include "CommunicationProtocolRequest.h"
#include "CommunicationProtocolResponse.h"

namespace Dic {
namespace Module {
class TextClusterDatabase : public VirtualClusterDatabase {
  public:
    explicit TextClusterDatabase(std::recursive_mutex &sqlMutex) : VirtualClusterDatabase(sqlMutex) {};
    ~TextClusterDatabase() noexcept override;

    bool SetConfig() override;
    bool SetDbVersion();
    bool CreateTable();
    bool CreateIndex();
    bool CreateTimeIndex();
    bool InitStmt();
    void ReleaseStmt();
    void InsertTimeInfo(CommunicationTimeInfo &timeInfo);
    void InsertTimeInfoList(std::vector<CommunicationTimeInfo> &timeInfoList);
    void InsertBandwidth(CommunicationBandWidth &bandWidth);
    void InsertBandwidthList(std::vector<CommunicationBandWidth> &bandWidthList);
    void InsertStepStatisticsInfo(StepStatistic &stepStatistic);
    void InsertClusterBaseInfo(ClusterBaseInfo &baseInfo);
    bool InsertGroupInfos(const std::vector<CommGroupParallelInfo> &groupInfos);
    bool BatchInsertDuplicateUpdateGroupInfo(const std::vector<CommGroupParallelInfo> &groupInfoList);
    bool InsertGroupInfoReturnIndex(const CommGroupParallelInfo &groupInfo, uint64_t &index);
    void InsertCommunicationMatrix(CommunicationMatrixInfo &communicationMatrix);
    void InsertCommunicationMatrixInfo(std::vector<CommunicationMatrixInfo> &matrixInfos);

    std::string QueryParseClusterStatus() override;
    void UpdateClusterParseStatus(std::string status) override;
    bool QueryBaseInfo(Protocol::SummaryBaseInfo &baseInfo) override;
    std::vector<std::string> GetAllRankFromStepStatisticInfo() override;
    std::vector<CommInfoUnderRank> GetCommTimeForRankDim(const std::string &stepId) override;
    bool QuerySlowOpByCommDuration(const Protocol::DurationListParams &params, const std::string &fastestRankId,
        Protocol::RankDetailsForSlowRank &slowRank) override;
    bool GetGroups(std::vector<GroupInfoDo> &groupList) override;
    bool QueryMatrixList(Protocol::MatrixBandwidthParam &param, std::vector<MatrixInfoDo> &matrixInfoDoList) override;
    bool QueryAllOperators(Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) override;
    bool QueryOperatorsCount(Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) override;
    bool QueryBandwidthData(Protocol::BandwidthDataParam &param, Protocol::BandwidthDataResBody &resBody) override;
    bool QueryDistributionData(Protocol::DistributionDataParam &param, Protocol::DistributionResBody &resBody) override;
    void SaveLastData();
    void SaveLastDataSafe();

    bool QueryRanksHandler(std::vector<Protocol::IterationsOrRanksObject> &responseBody) override;
    bool QueryOperatorNames(Protocol::OperatorNamesParams &requestParams,
        std::vector<Protocol::OperatorNamesObject> &responseBody) override;
    bool QueryMatrixSortOpNames(Protocol::OperatorNamesParams &requestParams,
        std::vector<Protocol::OperatorNamesObject> &responseBody) override;
    bool QueryIterations(std::vector<Protocol::IterationsOrRanksObject> &responseBody) override;
    bool QueryDurationList(
        Protocol::DurationListParams &requestParams, std::vector<DurationDo> &durationDoList) override;
    bool QueryOperatorList(
        Protocol::DurationListParams &requestParams, std::vector<OperatorTimeDo> &operatorTimeDoList) override;
    bool QueryExtremumTimestamp(uint64_t &min, uint64_t &max) override;
    bool UpdateCollectTimeInfo(const Protocol::SummaryBaseInfo &baseInfo) override;
    bool QueryIterationAndCommunicationGroup(
        Protocol::CommunicationKernelParams &params, Protocol::CommunicationKernelBody &responseBody) override;
    bool GetParallelConfigFromStepTrace(ParallelStrategyConfig &config, std::string &level) override;
    bool QueryParallelStrategyConfig(ParallelStrategyConfig &config, std::string &level) override;
    bool UpdateParallelStrategyConfig(
        const ParallelStrategyConfig &config, std::string &level, std::string &msg) override;
    std::map<std::string, std::string> QueryBaseInfoByKeys(const std::vector<std::string> &keys) override;
    bool InsertDuplicateUpdateBaseInfo(const std::map<std::string, std::string> &baseInfoMap) override;

    std::vector<CommGroupParallelInfo> GetAllGroupInfo();
    bool QueryAllPerformanceDataByStep(
        const std::string &step, std::unordered_map<std::uint32_t, StepStatistic> &data) override;

    bool QueryPacketAnalyzerData(std::vector<PacketAnalyzerData> &data) override;
    bool QueryBandwidthContentionAnalyzerData(
        std::vector<BandwidthContentionSDMAInfo> &res, const std::string &rankId) override;
    bool QueryRetransmissionAnalyzerData(std::vector<RetransmissionClassificationInfo> &data) override;
    std::vector<OpTypeStatistics> GetOpStatByStepId(const std::string &stepId) override;

  private:
    sqlite3_stmt *insertTimeInfoStmt = nullptr;
    sqlite3_stmt *insertBandwidthStmt = nullptr;
    sqlite3_stmt *stepStmt = nullptr;
    sqlite3_stmt *matrixStmt = nullptr;
    bool isInitStmt = false;
    // pp通信域
    std::vector<CommunicationTimeInfo> timeInfoCache;
    std::vector<CommunicationBandWidth> bandwidthCache;
    std::vector<CommunicationMatrixInfo> matrixCache;
    std::string GetTimeInfoStmtSql(int len);
    std::string GetBandwidthStmtSql(int len);
    std::string GetMatrixStmtSql(int len);

    std::string GetStageIdByGroupId(const std::string &groupId);
    std::string GetDurationListSql(const std::string &bandwidthCondition, const std::string &timeCondition) const;
    std::string GetAllOperatorsSql(
        const std::string &startTime, const std::string &bandwidthCondition, const std::string &timeCondition) const;
    std::string GetAllOperatorsSql(uint64_t &startTime, const Protocol::OperatorDetailsParam &param);
    void BindTextForClusterBaseInfo(ClusterBaseInfo &baseInfo, sqlite3_stmt *stmt);
};
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_JSON_CLUSTER_DATABASE_H
