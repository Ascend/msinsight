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

#ifndef PROFILER_SERVER_DBCLUSTERDATABASE_H
#define PROFILER_SERVER_DBCLUSTERDATABASE_H

#include "VirtualClusterDatabase.h"

namespace Dic {
namespace Module {
namespace FullDb {

class DbClusterDataBase : public VirtualClusterDatabase {
  public:
    explicit DbClusterDataBase(std::recursive_mutex &sqlMutex) : VirtualClusterDatabase(sqlMutex) {};
    ~DbClusterDataBase() override;

    bool CreateTable();
    bool DropTable();
    std::string QueryParseClusterStatus() override;
    void UpdateClusterParseStatus(std::string status) override;
    bool QueryBaseInfo(Protocol::SummaryBaseInfo &baseInfo) override;
    std::vector<std::string> GetAllRankFromStepStatisticInfo() override;
    bool QuerySlowOpByCommDuration(const Protocol::DurationListParams &params, const std::string &fastestRankId,
        Protocol::RankDetailsForSlowRank &slowRank) override;
    std::vector<CommInfoUnderRank> GetCommTimeForRankDim(const std::string &stepId) override;
    bool GetGroups(std::vector<GroupInfoDo> &groupList) override;
    bool QueryMatrixList(Protocol::MatrixBandwidthParam &param, std::vector<MatrixInfoDo> &matrixInfoDoList) override;
    bool QueryAllOperators(Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) override;
    bool QueryOperatorsCount(Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) override;
    bool QueryBandwidthData(Protocol::BandwidthDataParam &param, Protocol::BandwidthDataResBody &resBody) override;
    bool QueryDistributionData(Protocol::DistributionDataParam &param, Protocol::DistributionResBody &resBody) override;

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
    bool QueryAllPerformanceDataByStep(
        const std::string &step, std::unordered_map<std::uint32_t, StepStatistic> &data) override;

    void InsertClusterBaseInfo(ClusterBaseInfo &baseInfo);

    bool HasClusterBaseInfoTable() const;
    void SetHasClusterBaseInfoTable();
    bool QueryDistributedArgs(ParallelStrategyConfig &config, std::string &level);

    bool QueryPacketAnalyzerData(std::vector<PacketAnalyzerData> &data) override;
    bool QueryBandwidthContentionAnalyzerData(
        std::vector<BandwidthContentionSDMAInfo> &res, const std::string &rankId) override;
    bool QueryRetransmissionAnalyzerData(std::vector<RetransmissionClassificationInfo> &data) override;
    std::vector<OpTypeStatistics> GetOpStatByStepId(const std::string &stepId) override;

  private:
    std::string parseStatus = "UN_FINISH";
    // 标记初始状态的数据库是否有ClusterBaseInfo表
    bool hasClusterBaseInfoTable = false;
    bool ExecuteQueryDistributedArgs(ParallelStrategyConfig &config, std::string &level, std::string &sql);
};
}
}
}

#endif // PROFILER_SERVER_DBCLUSTERDATABASE_H
