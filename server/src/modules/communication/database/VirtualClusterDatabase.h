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

#ifndef PROFILER_SERVER_VIRTUAL_CLUSTER_DATABASE_H
#define PROFILER_SERVER_VIRTUAL_CLUSTER_DATABASE_H

#include <set>
#include <unordered_map>
#include "Database.h"
#include "ClusterDomainObject.h"
#include "ClusterDef.h"
#include "ProtocolMessage.h"
#include "SummaryProtocolResponse.h"
#include "TimelineProtocolResponse.h"
#include "TimelineProtocolRequest.h"
#include "SummaryProtocolRequest.h"
#include "CommunicationProtocolRequest.h"
#include "CommunicationProtocolResponse.h"
#include "TableDefs.h"

namespace Dic {
namespace Module {
class VirtualClusterDatabase : public Database {
  public:
    explicit VirtualClusterDatabase(std::recursive_mutex &sqlMutex) : Database(sqlMutex) {};
    ~VirtualClusterDatabase() override;

    virtual std::string QueryParseClusterStatus() = 0;
    virtual void UpdateClusterParseStatus(std::string status) = 0;
    bool HasFinishedParseLastTime();
    bool UpdatesClusterParseStatus(const std::string &status);

    virtual bool QueryBaseInfo(Protocol::SummaryBaseInfo &baseInfo) = 0;
    virtual std::vector<std::string> GetAllRankFromStepStatisticInfo() = 0;
    virtual std::vector<CommInfoUnderRank> GetCommTimeForRankDim(const std::string &stepId) = 0;
    virtual bool QuerySlowOpByCommDuration(const Protocol::DurationListParams &params, const std::string &fastestRankId,
        Protocol::RankDetailsForSlowRank &slowRank) = 0;
    virtual bool GetGroups(std::vector<GroupInfoDo> &groupList) = 0;
    virtual bool QueryMatrixList(
        Protocol::MatrixBandwidthParam &param, std::vector<MatrixInfoDo> &matrixInfoDoList) = 0;
    virtual bool QueryAllOperators(
        Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) = 0;
    virtual bool QueryOperatorsCount(
        Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) = 0;
    virtual bool QueryBandwidthData(Protocol::BandwidthDataParam &param, Protocol::BandwidthDataResBody &resBody) = 0;
    virtual bool QueryDistributionData(
        Protocol::DistributionDataParam &param, Protocol::DistributionResBody &resBody) = 0;

    virtual bool QueryRanksHandler(std::vector<Protocol::IterationsOrRanksObject> &responseBody) = 0;
    virtual bool QueryOperatorNames(
        Protocol::OperatorNamesParams &requestParams, std::vector<Protocol::OperatorNamesObject> &responseBody) = 0;
    virtual bool QueryIterations(std::vector<Protocol::IterationsOrRanksObject> &responseBody) = 0;
    virtual bool QueryDurationList(
        Protocol::DurationListParams &requestParams, std::vector<DurationDo> &durationDoList) = 0;
    virtual bool QueryOperatorList(
        Protocol::DurationListParams &requestParams, std::vector<OperatorTimeDo> &operatorTimeDoList) = 0;
    virtual bool QueryMatrixSortOpNames(
        Protocol::OperatorNamesParams &requestParams, std::vector<Protocol::OperatorNamesObject> &responseBody) = 0;
    virtual bool QueryExtremumTimestamp(uint64_t &min, uint64_t &max) = 0;
    virtual bool UpdateCollectTimeInfo(const Protocol::SummaryBaseInfo &baseInfo) = 0;
    virtual bool QueryIterationAndCommunicationGroup(
        Protocol::CommunicationKernelParams &params, Protocol::CommunicationKernelBody &responseBody) = 0;
    virtual bool GetParallelConfigFromStepTrace(ParallelStrategyConfig &config, std::string &level) = 0;
    virtual bool QueryParallelStrategyConfig(ParallelStrategyConfig &config, std::string &level) = 0;
    virtual bool UpdateParallelStrategyConfig(
        const ParallelStrategyConfig &config, std::string &level, std::string &msg) = 0;
    virtual std::map<std::string, std::string> QueryBaseInfoByKeys(const std::vector<std::string> &keys) = 0;
    virtual bool InsertDuplicateUpdateBaseInfo(const std::map<std::string, std::string> &baseInfoMap) = 0;
    virtual bool QueryAllPerformanceDataByStep(
        const std::string &step, std::unordered_map<std::uint32_t, StepStatistic> &data) = 0;
    virtual bool QueryPacketAnalyzerData(std::vector<PacketAnalyzerData> &data) = 0;
    virtual bool QueryBandwidthContentionAnalyzerData(
        std::vector<BandwidthContentionSDMAInfo> &res, const std::string &rankId) = 0;
    virtual bool QueryRetransmissionAnalyzerData(std::vector<RetransmissionClassificationInfo> &data) = 0;
    virtual std::vector<OpTypeStatistics> GetOpStatByStepId(const std::string &stepId) = 0;
    bool BatchInsertExpertHotspotData(const std::vector<ExpertHotspotStruct> &expertHotspotInfos);
    bool BatchInsertExpertDeployment(const std::vector<ExpertDeploymentStruct> &expertDeploymentInfos);
    void InsertExpertHotspotDataForCache(const ExpertHotspotStruct &info);
    void InsertExpertDeploymentForCache(const ExpertDeploymentStruct &info);
    void SaveExpertHotspot();
    void SaveExpertDeployment();
    bool DeleteExpertHotspot(const std::string &modelStage, const std::string &version);
    bool DeleteDeployment(const std::string &modelStage, const std::string &version);
    std::vector<ExpertDeploymentStruct> QueryExpertDeployment(
        const std::string &modelStage, const std::string &version);
    std::vector<ExpertHotspotStruct> QueryExpertHotspotData(const std::string &modelStage, const std::string &version);
    void ReleaseStmt();

  protected:
    const static inline int doubleReservedNum = 3;
    const static inline int slowRankOpCount = 3;
    const std::string totalOpInfo = "Total Op Info";
    const std::string clusterParseStatus = "Cluster files parsing status";
    const std::string commonSql = "CREATE TABLE IF NOT EXISTS " + TABLE_EXPERT_HOTSPOT_INTO +
        " (id INTEGER PRIMARY KEY AUTOINCREMENT, localExpertId INTEGER, modelStage TEXT, rankId INTEGER,"
        " visits INTEGER, layer INTEGER, version TEXT);"
        " CREATE INDEX IF NOT EXISTS idx_ms ON " +
        TABLE_EXPERT_HOTSPOT_INTO +
        "(modelStage, version);"
        " CREATE TABLE IF NOT EXISTS " +
        TABLE_EXPERT_DEPLOYMENT_INFO +
        " (id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " modelStage TEXT, rankId INTEGER, layer INTEGER, expertList TEXT, version TEXT);"
        " CREATE INDEX IF NOT EXISTS idx_ms ON " +
        TABLE_EXPERT_DEPLOYMENT_INFO + "(modelStage, version)";
    const std::vector<std::string> replaceCharList = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "_"};
    sqlite3_stmt *insertHotspotStmt = nullptr;
    sqlite3_stmt *insertDeploymentStmt = nullptr;
    std::vector<ExpertHotspotStruct> expertHotspotCache;
    std::vector<ExpertDeploymentStruct> expertDeploymentCache;
    const double overlapThreshold = 0.05;
    bool HasColumn(const std::string &tableName, const std::string &columnName);
    bool ExecuteQueryBaseInfo(Protocol::SummaryBaseInfo &baseInfo, std::string sql);
    std::map<std::string, std::string> ExecuteQueryBaseInfoByKeys(
        const std::vector<std::string> &keys, const std::string &tableName);
    bool ExecuteInsertDuplicateUpdateBaseInfo(
        const std::map<std::string, std::string> &baseInfoMap, const std::string &tableName);
    std::vector<std::string> ExecuteGetAllRankFromStepStatisticInfo(std::string &sql);
    bool ExecuteQuerySlowOpByCommDuration(const std::string &sql, const Protocol::DurationListParams &params,
        const std::string &fastestRankId, Protocol::RankDetailsForSlowRank &slowRank);
    std::vector<CommInfoUnderRank> ExecuteGetCommTimeForRankDim(std::string &sql, const std::string &step);
    bool ExecuteGetGroups(std::vector<GroupInfoDo> &groupList, const std::string &sql);
    bool ExecuteQueryMatrixList(
        Protocol::MatrixBandwidthParam &param, std::vector<MatrixInfoDo> &matrixInfoDoList, const std::string &sql);
    bool ExecuteQueryAllOperators(Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody,
        std::string sql, uint64_t startTime);
    bool ExecuteQueryOperatorsCount(
        Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody, std::string sql);
    bool ExecuteQueryBandwidthData(
        Protocol::BandwidthDataParam &param, Protocol::BandwidthDataResBody &resBody, std::string sql);
    bool ExecuteQueryDistributionData(
        Protocol::DistributionDataParam &param, Protocol::DistributionResBody &resBody, std::string sql);

    bool ExecuteQueryRanksHandler(std::vector<Protocol::IterationsOrRanksObject> &responseBody, std::string sql);
    bool ExecuteQueryOperatorNames(Protocol::OperatorNamesParams &requestParams,
        std::vector<Protocol::OperatorNamesObject> &responseBody, std::string sql);
    bool ExecuteQueryIterations(std::vector<Protocol::IterationsOrRanksObject> &responseBody, std::string sql);
    bool ExecuteQueryDurationList(Protocol::DurationListParams &requestParams, std::vector<DurationDo> &durationDoList,
        std::string sql, uint64_t startTime);
    bool ExecuteQueryOperatorList(Protocol::DurationListParams &requestParams,
        std::vector<OperatorTimeDo> &operatorTimeDoList, const std::string &sql, uint64_t startTime);
    bool ExecuteQueryMatrixSortOpNames(Protocol::OperatorNamesParams &requestParams,
        std::vector<Protocol::OperatorNamesObject> &responseBody, std::string sql);
    std::string GetRanksSql(const std::vector<std::string> &rankList);

    bool ExecuteQueryExtremumTimestamp(std::string &sql, uint64_t &min, uint64_t &max);
    bool ExecuteQueryIterationAndCommunicationGroup(std::string &sql, std::string &opName, const std::string &rankId,
        std::string &iteration, std::string &communicationGroup);
    bool ExecuteGetParallelConfigFromStepTrace(std::string &sql, ParallelStrategyConfig &config, std::string &level);
    bool ExecuteQueryParallelStrategyConfig(std::string &sql, ParallelStrategyConfig &config, std::string &level);
    bool ExecuteSetParallelStrategyConfig(std::string &sql, const ParallelStrategyConfig &config, std::string &level);
    bool ExecuteQueryAllPerformanceDataByStep(
        const std::string &sql, const std::string &step, std::unordered_map<std::uint32_t, StepStatistic> &data);
    void GetStepsOrRanksObject(
        const std::string &jsonStr, std::vector<Protocol::IterationsOrRanksObject> &responseBody);

    bool ExecuteQueryPacketAnalyzerData(std::vector<PacketAnalyzerData> &data, const std::string &sql);
    bool ExecuteQueryBandwidthContentionAnalyzerData(
        std::vector<BandwidthContentionSDMAInfo> &res, const std::string &rankId, const std::string &sql);
    bool ExecuteQueryRetransmissionAnalyzerData(
        std::vector<RetransmissionClassificationInfo> &data, const std::string &sql);
    bool ExecuteUpdateCollectTimeInfo(const Protocol::SummaryBaseInfo &baseInfo, const std::string &sql);
    std::string GenerateReplaceSql(const std::string &columnName, const std::vector<std::string> &replaceList);
    std::vector<OpTypeStatistics> ExecuteGetOpStatByStepId(const std::string &stepId, const std::string &sql);

    sqlite3_stmt *GetExpertHotspotInsertStmt(uint64_t paramLen);
    sqlite3_stmt *InitExpertHotspotInsertStmt(uint64_t paramLen);
    sqlite3_stmt *GetExpertDeploymentInsertStmt(uint64_t paramLen);
    sqlite3_stmt *InitExpertDeploymentInsertStmt(uint64_t paramLen);
};
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_VIRTUAL_CLUSTER_DATABASE_H
