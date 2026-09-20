/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

// Test-only adapter: unrelated trace queries are inert; prepared statements use real SQLite.
#ifndef INSIGHT_MEMCPY_DETAIL_TEST_DATABASE_H
#define INSIGHT_MEMCPY_DETAIL_TEST_DATABASE_H
#include "VirtualTraceDatabase.h"
namespace Dic::Module::Timeline {
class MemcpyDetailTestDatabase final : public VirtualTraceDatabase {
  public:
    explicit MemcpyDetailTestDatabase(std::recursive_mutex &mutex) : VirtualTraceDatabase(mutex) {}
    bool OpenMemory() {
        if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
            return false;
        }
        isOpen = true;
        path = ":memory:";
        return true;
    }
    sqlite3 *Handle() const { return db; }
    size_t prepareCount = 0;
    std::vector<std::string> preparedSql;
    std::unique_ptr<SqlitePreparedStatement> CreatPreparedStatement(const std::string &sql) override {
        ++prepareCount;
        preparedSql.push_back(sql);
        return Database::CreatPreparedStatement(sql);
    }
    bool QueryGroupedAscendHardwareThreadsByModelId(std::vector<ThreadGroup> &) override { return false; }
    bool QueryThreads(const Protocol::UnitThreadsParams &requestParams, Protocol::UnitThreadsBody &responseBody,
        uint64_t minTimestamp, const std::vector<uint64_t> &trackIdList) override {
        return {};
    }
    bool QueryThreadTracesSummary(const Protocol::UnitThreadTracesSummaryParams &requestParams,
        Protocol::UnitThreadTracesSummaryBody &responseBody, uint64_t minTimestamp) override {
        return {};
    }
    std::map<std::string, std::string> QueryAllModelIdOfAscendHardwareThreads() override { return {}; }
    bool QueryUnitsMetadata(
        const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) override {
        return {};
    }
    bool QueryExtremumTimestamp(uint64_t &min, uint64_t &max) override { return {}; }
    bool QueryUnitFlows(const Protocol::UnitFlowsParams &requestParams, Protocol::UnitFlowsBody &responseBody,
        uint64_t minTimestamp, uint64_t trackId) override {
        return {};
    }
    bool SetCardAlias(
        const Protocol::SetCardAliasParams &requestParams, Protocol::SetCardAliasBody &responseBody) override {
        return {};
    }
    std::string QueryCardAlias() override { return {}; }
    uint32_t SearchSliceNameCount(
        const Protocol::SearchCountParams &params, const std::vector<TrackQuery> &trackQuery) override {
        return {};
    }
    bool SearchSliceName(const Protocol::SearchSliceParams &params, int index, uint64_t minTimestamp,
        Protocol::SearchSliceBody &responseBody, const std::vector<TrackQuery> &trackQuery) override {
        return {};
    }
    bool QueryHostSlicesByName(const std::string &sliceName, const std::string &metaType,
        std::vector<Protocol::SimpleSlice> &result) override {
        return {};
    }
    bool QueryDeviceSlicesByName(const std::string &rankId, const std::string &sliceName, const std::string &metaType,
        std::vector<Protocol::SimpleSlice> &result) override {
        return {};
    }
    bool QueryTextSlicesByName(const std::string &sliceName, const std::string &metaType,
        std::vector<Protocol::SimpleSlice> &result) override {
        return {};
    }
    bool QueryFlowCategoryList(std::vector<std::string> &categories, const std::string &rankId) override { return {}; }
    bool QueryUnitCounter(Protocol::UnitCounterParams &params, uint64_t minTimestamp,
        std::vector<Protocol::UnitCounterData> &dataList) override {
        return {};
    }
    bool QueryComputeStatisticsData(
        const Protocol::SummaryStatisticParams &requestParams, Protocol::SummaryStatisticsBody &responseBody) override {
        return {};
    }
    bool QueryCommunicationStatisticsData(
        const Protocol::SummaryStatisticParams &requestParams, Protocol::SummaryStatisticsBody &responseBody) override {
        return {};
    }
    bool QueryStepDuration(const std::string &stepId, uint64_t &min, uint64_t &max) override { return {}; }
    bool QuerySystemViewData(const Protocol::SystemViewParams &requestParams, Protocol::SystemViewBody &responseBody,
        const uint64_t &minTimestamp) override {
        return {};
    }
    bool QuerySystemViewTraceData(const Protocol::SystemViewParams &requestParams,
        Protocol::SystemViewTraceBody &responseBody, const uint64_t &minTimestamp) override {
        return {};
    }
    bool QueryExpAnaAICoreFreqData(const Protocol::SystemViewAICoreFreqParams &requestParams,
        Protocol::ExpAnaAICoreFreqBody &responseBody, std::vector<std::pair<uint64_t, uint64_t>> &freqs,
        uint64_t &maxFreq, uint64_t &minFreq) override {
        return {};
    }
    LayerStatData QueryLayerData(const Protocol::SystemViewParams &requestParams, const std::string &name,
        const uint64_t &minTimestamp, const std::string &timeRangeConditionSql) override {
        return {};
    }
    std::vector<std::string> QueryCoreType() override { return {}; }
    bool QueryKernelDetailData(const Protocol::KernelDetailsParams &requestParams,
        Protocol::KernelDetailsBody &responseBody, uint64_t minTimestamp) override {
        return {};
    }
    uint64_t QueryTotalKernel(const Protocol::KernelDetailsParams &requestParams, uint64_t minTimestamp) override {
        return {};
    }
    bool QueryKernelDepthAndThread(
        const Protocol::KernelParams &params, Protocol::OneKernelBody &responseBody, uint64_t minTimestamp) override {
        return {};
    }
    bool QueryCommunicationKernelInfo(
        const std::string &name, const std::string &rankId, Protocol::CommunicationKernelBody &body) override {
        return {};
    }
    OneKernelData QueryKernelTid(uint64_t trackId) override { return {}; }
    bool SearchAllSlicesDetails(const Protocol::SearchAllSliceParams &params, Protocol::SearchAllSlicesBody &body,
        uint64_t minTimestamp, const std::vector<TrackQuery> &trackQueryVec) override {
        return {};
    }
    bool LoadSliceCache(
        LightSliceCache &cache, const Protocol::SearchAllSliceParams &params, uint64_t minTimestamp) override {
        return {};
    }
    bool FetchSliceDetails(const LightSliceCache &cache, const std::vector<TargetRow> &rows,
        const Protocol::SearchAllSliceParams &params, Protocol::SearchAllSlicesBody &body,
        uint64_t minTimestamp) override {
        return {};
    }
    bool QueryAffinityOptimizer(const Protocol::KernelDetailsParams &params, const std::string &optimizers,
        std::vector<Protocol::ThreadTraces> &data, uint64_t minTimestamp) override {
        return {};
    }
    bool QueryThreadSameOperatorsDetails(const Protocol::UnitThreadsOperatorsParams &requestParams,
        Protocol::UnitThreadsOperatorsBody &responseBody, uint64_t minTimestamp,
        const std::vector<uint64_t> &trackIdList) override {
        return {};
    }
    bool QueryAICpuOpCanBeOptimized(const Protocol::KernelDetailsParams &params,
        const std::vector<std::string> &replace, const std::map<std::string, Timeline::AICpuCheckDataType> &dataType,
        std::vector<Protocol::KernelBaseInfo> &data, uint64_t minTimestamp) override {
        return {};
    }
    bool QueryAclnnOpCountExceedThreshold(const Protocol::KernelDetailsParams &params, uint64_t threshold,
        std::vector<Protocol::KernelBaseInfo> &data, uint64_t minTimestamp) override {
        return {};
    }
    bool QueryAffinityAPIData(const Protocol::KernelDetailsParams &params, const std::set<std::string> &pattern,
        uint64_t minTimestamp, std::map<uint64_t, std::vector<Protocol::FlowLocation>> &data,
        std::map<uint64_t, std::vector<uint32_t>> &indexs) override {
        return {};
    }
    bool QueryFusibleOpData(const Protocol::KernelDetailsParams &params,
        const std::vector<Timeline::FuseableOpRule> &rule, Protocol::OperatorFusionResBody &resBody,
        uint64_t minTimestamp) override {
        return {};
    }
    bool QueryOperatorDispatchData(const Protocol::KernelDetailsParams &params,
        std::vector<Protocol::KernelBaseInfo> &data, uint64_t minTimestamp, uint64_t threshold) override {
        return {};
    }
    bool QueryEventsViewData(
        const Protocol::EventsViewParams &params, Protocol::EventsViewBody &body, uint64_t minTimestamp) override {
        return {};
    }
    std::string QueryHostInfo() override { return {}; }
    bool QueryFwdBwdDataByFlow(const std::string &rankId, uint64_t offset, const Protocol::ExtremumTimestamp &range,
        std::vector<Protocol::ThreadTraces> &fwdBwdData) override {
        return {};
    }
    bool QueryP2PCommunicationOpData(const std::string &rankId, uint64_t offset,
        const Protocol::ExtremumTimestamp &range, std::vector<Protocol::ThreadTraces> &p2pOpData) override {
        return {};
    }
    bool QueryByteAlignmentAnalyzerData(std::vector<CommunicationLargeOperatorInfo> &data) override { return {}; }
};
}
#endif
