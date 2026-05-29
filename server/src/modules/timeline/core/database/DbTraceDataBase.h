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

#ifndef PROFILER_SERVER_DBTRACEDATABASE_H
#define PROFILER_SERVER_DBTRACEDATABASE_H

#include "VirtualTraceDatabase.h"
#include "TraceDatabaseDef.h"
#include "map"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"
#include "DomainObject.h"
#include "SliceAnalyzer.h"
#include "FlowAnalyzer.h"

// clang-format off
namespace Dic::Module::FullDb {
using namespace Dic::Module::Timeline;

// SQL 查询参数结构体
struct SearchSliceSqlParams {
    bool isMatchExact = false;
    bool isMatchCase = false;
    std::string order;
    std::string orderByField;
    std::string rankId;
    std::string nameFilter;  // 二级筛选，空表示无筛选
};

struct TASK_INFO {
    uint64_t start = 0;
    uint64_t end = 0;
    uint64_t depth = 0;
    uint64_t id = 0;
};

struct WAIT_TIME {
    int64_t waitTime = 0;
    int64_t id = 0;
    std::string type;
};

struct OVERLAP_INFO {
    OVERLAP_INFO() = default;
    OVERLAP_INFO(int64_t startNs, int64_t endNs, int64_t type) : startNs(startNs), endNs(endNs), type(type) {};
    int64_t startNs;
    int64_t endNs;
    int64_t type; // Computing = 0, Communication = 1, Communication(Not Overlapped) = 2, Free = 3
    bool operator<(const OVERLAP_INFO &right) const {
        if (startNs < right.startNs) {
            return true;
        }
        if (startNs == right.startNs && endNs < right.endNs) {
            return true;
        }
        return false;
    }
};

struct QUERY_THREAD_SAME_OPERATORS_PARAMS {
    std::string rankId;
    uint64_t minTimestamp;
    std::string orderBy;
    std::vector<std::string> pidList;
    std::vector<std::string> tidList;
};
class DbTraceDataBase : public VirtualTraceDatabase {
  public:
    explicit DbTraceDataBase(std::recursive_mutex &sqlMutex);
    ~DbTraceDataBase();

    bool OpenDb(const std::string &dbPath, bool clearAllTable) override;
    void AddHelperColumnsAndSetStatus();
    bool InitStmt();

    // search
    bool QueryThreads(const Protocol::UnitThreadsParams &requestParams, Protocol::UnitThreadsBody &responseBody,
        uint64_t minTimestamp, const std::vector<uint64_t> &trackIdList) override;
    std::map<std::string, std::string> QueryAllModelIdOfAscendHardwareThreads() override;
    bool QueryUnitsMetadata(
        const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) override;
    bool QueryExtremumTimestamp(uint64_t &min, uint64_t &max) override;
    bool QueryUnitFlows(const Protocol::UnitFlowsParams &requestParams, Protocol::UnitFlowsBody &responseBody,
        uint64_t minTimestamp, uint64_t trackId) override;
    bool SetCardAlias(
        const Protocol::SetCardAliasParams &requestParams, Protocol::SetCardAliasBody &responseBody) override;
    std::string QueryCardAlias() override;
    uint32_t SearchSliceNameCount(
        const Protocol::SearchCountParams &params, const std::vector<TrackQuery> &trackQuery) override;
    bool SearchSliceName(const Protocol::SearchSliceParams &params, int index, uint64_t minTimestamp,
        Protocol::SearchSliceBody &responseBody, const std::vector<TrackQuery> &trackQuery) override;
    bool QueryHostSlicesByName(const std::string &sliceName, const std::string &metaType,
        std::vector<Protocol::SimpleSlice> &result, std::set<std::string> &processIds) override;
    bool QueryDeviceSlicesByName(const std::string &rankId, const std::string &sliceName, const std::string &metaType,
        std::vector<Protocol::SimpleSlice> &result, std::set<std::string> &processIds) override;
    bool QueryTextSlicesByName(const std::string &sliceName, const std::string &metaType,
        std::vector<Protocol::SimpleSlice> &result, std::set<std::string> &processIds) override;
    bool QueryFlowCategoryList(std::vector<std::string> &categories, const std::string &rankId) override;
    bool QueryUnitCounter(Protocol::UnitCounterParams &params, uint64_t minTimestamp,
        std::vector<Protocol::UnitCounterData> &dataList) override;
    bool QueryComputeStatisticsData(
        const Protocol::SummaryStatisticParams &requestParams, Protocol::SummaryStatisticsBody &responseBody) override;
    bool QueryCommunicationStatisticsData(
        const Protocol::SummaryStatisticParams &requestParams, Protocol::SummaryStatisticsBody &responseBody) override;
    bool QueryStepDuration(const std::string &stepId, uint64_t &min, uint64_t &max) override;
    bool QuerySystemViewData(const Protocol::SystemViewParams &requestParams, Protocol::SystemViewBody &responseBody,
        const uint64_t &minTimestamp) override;
    bool QueryExpAnaAICoreFreqData(const Protocol::SystemViewAICoreFreqParams &requestParams,
        Protocol::ExpAnaAICoreFreqBody &responseBody, std::vector<std::pair<uint64_t, uint64_t>> &freqs,
        uint64_t &maxFreq, uint64_t &minFreq) override;
    LayerStatData QueryLayerData(const Protocol::SystemViewParams &requestParams, const std::string &name,
        const uint64_t &minTimestamp, const std::string &timeRangeConditionSql) override;
    std::vector<std::string> QueryCoreType() override;
    bool QueryKernelDetailData(const Protocol::KernelDetailsParams &requestParams,
        Protocol::KernelDetailsBody &responseBody, uint64_t minTimestamp) override;
    uint64_t QueryTotalKernel(const Protocol::KernelDetailsParams &requestParams, uint64_t minTimestamp) override;
    bool QueryKernelDepthAndThread(
        const Protocol::KernelParams &params, Protocol::OneKernelBody &responseBody, uint64_t minTimestamp) override;
    bool QueryCommunicationKernelInfo(
        const std::string &name, const std::string &rankId, Protocol::CommunicationKernelBody &body) override;
    OneKernelData QueryKernelTid(uint64_t trackId) override;
    bool QueryThreadTracesSummary(const Protocol::UnitThreadTracesSummaryParams &requestParams,
        Protocol::UnitThreadTracesSummaryBody &responseBody, uint64_t minTimestamp) override;

    bool SearchAllSlicesDetails(const Protocol::SearchAllSliceParams &params, Protocol::SearchAllSlicesBody &body,
        uint64_t minTimestamp, const std::vector<TrackQuery> &trackQueryVec) override;

    // 【新增】加载 SoA 缓存
    bool LoadSliceCache(LightSliceCache& cache,
        const Protocol::SearchAllSliceParams& params, uint64_t minTimestamp) override;

    // 【新增】根据 TargetRow 查询明细数据（使用 SQL 模板工厂）
    bool FetchSliceDetails(const LightSliceCache& cache,
        const std::vector<TargetRow>& rows,
        const Protocol::SearchAllSliceParams& params,
        Protocol::SearchAllSlicesBody& body, uint64_t minTimestamp) override;

    bool QueryThreadSameOperatorsDetails(const Protocol::UnitThreadsOperatorsParams &requestParams,
        Protocol::UnitThreadsOperatorsBody &responseBody, uint64_t minTimestamp,
        const std::vector<uint64_t> &trackIdList) override;
    bool QueryHostMetadata(const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData);

    std::vector<std::string> QueryRankId();
    std::string QueryHostInfo() override;
    std::string QueryHostInfoWithHostPath(const std::string &path);
    std::string GetDeviceId(const std::string &rankIdWithHost);
    std::string GetDeviceIdFromMemoryTable();
    std::unordered_map<std::string, std::string> QueryRankIdAndDeviceMap();

    bool QueryAffinityOptimizer(const Protocol::KernelDetailsParams &params, const std::string &optimizers,
        std::vector<Protocol::ThreadTraces> &data, uint64_t minTimestamp) override;
    bool QueryAICpuOpCanBeOptimized(const Protocol::KernelDetailsParams &params,
        const std::vector<std::string> &replace, const std::map<std::string, Timeline::AICpuCheckDataType> &dataType,
        std::vector<Protocol::KernelBaseInfo> &data, uint64_t minTimestamp) override;
    bool QueryAclnnOpCountExceedThreshold(const Protocol::KernelDetailsParams &params, uint64_t threshold,
        std::vector<Protocol::KernelBaseInfo> &data, uint64_t minTimestamp) override;
    bool QueryAffinityAPIData(const Protocol::KernelDetailsParams &params, const std::set<std::string> &pattern,
        uint64_t minTimestamp, std::map<uint64_t, std::vector<Protocol::FlowLocation>> &data,
        std::map<uint64_t, std::vector<uint32_t>> &indexes) override;
    bool QueryFusibleOpData(const Protocol::KernelDetailsParams &params,
        const std::vector<Timeline::FuseableOpRule> &rule, Protocol::OperatorFusionResBody &resBody,
        uint64_t minTimestamp) override;
    bool QueryOperatorDispatchData(const Protocol::KernelDetailsParams &params,
        std::vector<Protocol::KernelBaseInfo> &data, uint64_t minTimestamp, uint64_t threshold) override;
    bool QueryEventsViewData(
        const Protocol::EventsViewParams &params, Protocol::EventsViewBody &body, uint64_t minTimestamp) override;

    bool CheckTableDataInvalid(std::string tableName);

    void UpdateStartTime(const std::string &fileId);
    static void ClearStringsCache();
    void InitStringsCache();
    void InitMetaDataInfo();
    static std::string GetStringCacheValue(const std::string &path, const std::string &key);
    static std::vector<std::string> GetIdListByFuzzNameFromCache(
        const std::string &path, const std::string &fuzzName, const bool caseSensitive = true);
    bool InitConnectionCats();
    void UpdateWaitTime();
    bool GenerateOverlapAnalysis();
    std::vector<OVERLAP_INFO> BuildOverlapInfoList(
        const std::vector<OVERLAP_INFO> &timeInfoList, const std::string &deviceId);
    bool QueryTaskEarliestAndLatestTimeExcludingCertainEvent(
        std::pair<int64_t, int64_t> &time, const std::string &deviceId);
    static void Reset();
    bool QueryFwdBwdDataByFlow(const std::string &rankId, uint64_t offset, const Protocol::ExtremumTimestamp &range,
        std::vector<Protocol::ThreadTraces> &fwdBwdData) override;
    bool QueryP2PCommunicationOpData(const std::string &rankId, uint64_t offset,
        const Protocol::ExtremumTimestamp &range, std::vector<Protocol::ThreadTraces> &p2pOpData) override;
    bool QueryByteAlignmentAnalyzerData(std::vector<CommunicationLargeOperatorInfo> &data) override;
    bool QueryByteAlignmentAnalyzerRawData(std::vector<ByteAlignmentAnalyzerLargeOperatorInfo> &largeOpInfo,
        std::vector<ByteAlignmentAnalyzerSmallOperatorInfo> &smallOpInfo);
    static std::string GetHostPath(const std::string &filePath);

    void AddPythonStackMetadata(const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData);
    void ProcessHostCounterEventsMetadata(
        const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData);
    void QueryDeviceIdInStepTraceTime(std::set<std::string> &deviceIds);

    bool QueryFwdBwdFromMstx(std::vector<Protocol::ThreadTraces> &traceList) override;
    bool QueryP2PCommunicationOpHaveConnectionId(std::vector<Protocol::ThreadTraces> &traceList) override;

    static std::string GetSearchSliceNameSql(
        bool isMatchExact, bool isMatchCase, const std::string &rankId, const std::string &path);

    static std::string GetSearchAllSlicesDetailsSql(const SearchSliceSqlParams &params);

    static std::string GetSearchSliceNameCountSql(const SearchSliceSqlParams &params);

    static std::string GetSearchCountWithLockSql(
        const SearchCountParams &params, const std::vector<TrackQuery> &trackQuery);

    static bool QueryFusibleOpDataForDB(const KernelDetailsParams &params,
        std::unique_ptr<SqlitePreparedStatement> &stmt, const std::vector<Timeline::FuseableOpRule> &rule,
        Protocol::OperatorFusionResBody &resBody, uint64_t minTimestamp);

    static bool QueryOpDispatchDataForDB(std::unique_ptr<SqlitePreparedStatement> &stmt, uint64_t minTimestamp,
        const KernelDetailsParams &params, uint64_t threshold, std::vector<KernelBaseInfo> &data);

    static void ProcessByteAlignmentAnalyzerDataForDb(std::vector<CommunicationLargeOperatorInfo> &result,
        std::vector<ByteAlignmentAnalyzerLargeOperatorInfo> &largeOpInfo,
        std::vector<ByteAlignmentAnalyzerSmallOperatorInfo> &smallOpInfo);

    static std::string GetKernelDetailSql(
        const KernelDetailsParams &requestParams, const std::string &blockNumColumnName);

  private:
    const uint32_t cacheSize = 5000;
    bool initStmt = false;
    bool isExistPytorch = false;
    bool isExistCANN = false;
    bool isExistMstx = false;
    bool isExistCommOp = false;
    bool isExistTask = false;
    bool isExistComputeTask = false;

    std::string host;
    std::string hostPath; // 记录到 card 之前的文件路径
    const std::string cardAliasName = "RANK_LABEL";

    std::unique_ptr<SqlitePreparedStatement> updateTaskDepthStmt = nullptr;
    std::unique_ptr<SqlitePreparedStatement> updateApiDepthStmt = nullptr;
    std::unique_ptr<SqlitePreparedStatement> updateCANNApiDepthStmt = nullptr;
    std::unique_ptr<SqlitePreparedStatement> insertOverlapStmt = nullptr;

    std::vector<TASK_INFO> taskDepthCache;
    std::vector<WAIT_TIME> taskWaitTimeCache;
    std::vector<OVERLAP_INFO> timeInfoCache;
    std::vector<std::string> rankIds;

    std::unique_ptr<SliceAnalyzer> sliceAnalyzerPtr = nullptr;
    std::unique_ptr<FlowAnalyzer> flowAnalyzerPtr = nullptr;

    bool SetConfig() override;
    void CreateTemporaryTable();

    std::string GetHcclOperatorMetaData(const std::string &fileId);
    bool QueryAscendHardwareOperatorMetadata(
        const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData);
    bool QueryHCCLOperatorMetadata(
        const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData);
    bool GenerateOverlapAnalysisMetadata(
        const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData);
    bool QueryCounterMetadata(const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData);
    bool QueryCounterMetadataGenerateInfo(
        const PROCESS_TYPE &type, std::string &processName, std::string &metaType, std::string &sql);

    static constexpr const char *HARDWARE_METRICS_PROCESS_ID = "__hardware_metrics__";
    static constexpr const char *HARDWARE_METRICS_PROCESS_NAME = "Hardware Metrics";
    static constexpr const char *HARDWARE_METRICS_META_TYPE = "HARDWARE_METRICS";
    static bool IsHardwareMetricsUnit(const std::string &processId, const std::string &metaType);
    static std::unique_ptr<Protocol::UnitTrack> GenerateHardwareMetricsUnitTrack(const std::string &fileId);
    static Protocol::UnitTrack *GetOrCreateHardwareMetricsUnitTrack(
        const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData);

    static std::unique_ptr<Protocol::UnitTrack> GenerateBaseUnitTrack(const std::string &type,
        const std::string &cardId, const std::string &processId, const std::string &processName,
        const std::string &metaType);
    void DealHostMetadata(const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData,
        std::map<std::string, std::vector<MetaDataDto>> &threadMap);
    bool UpdateTaskInfoWaitTime(std::unique_ptr<SqlitePreparedStatement> &updateComputeStmt,
        std::unique_ptr<SqlitePreparedStatement> &updateCommunicationStmt);
    void QueryTaskTimeInfo(bool isComputing, std::vector<OVERLAP_INFO> &timeInfoList, const std::string &deviceId);
    bool InsertOverlapAnalysisInfo(const std::vector<OVERLAP_INFO> &overlapInfoList, const std::string &rankId);
    std::vector<Protocol::SimpleSlice> QueryThreadByPid(const Protocol::Metadata &metaData, uint64_t startTime,
        uint64_t endTime, const std::string &rankId, std::map<std::string, uint64_t> &selfTimeKeyValue);

    void ProcessThreadUnit(std::unique_ptr<Protocol::UnitTrack> &process, std::unique_ptr<SqliteResultSet> &resultSet,
        std::unique_ptr<Protocol::UnitTrack> &thread, const std::string &threadId, const PROCESS_TYPE &type) const;
    bool ExcecuteQueryKernelDetailData(std::unique_ptr<SqlitePreparedStatement> &stmt,
        const Protocol::KernelDetailsParams &requestParams, Protocol::KernelDetailsBody &responseBody,
        uint64_t minTimestamp, const std::string &blockNumColumnName);
    void ExecuteQueryDbThreadSameOperatorsDetails(const std::unique_ptr<SqliteResultSet> &resultSet,
        const Protocol::UnitThreadsOperatorsParams &requestParams, Protocol::UnitThreadsOperatorsBody &responseBody,
        const std::vector<std::string> tidList);

    void UpdataCommucationThreadName(const PROCESS_TYPE &type, std::unique_ptr<Protocol::UnitTrack> &process) const;
    void FillFlowDepth(const Protocol::UnitFlowsParams &requestParams, FlowLocation &location,
        std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint32_t>> &trackIdDepthCache);

    // 点击单个算子显示连线相关方法
    std::vector<FlowLocation> ExecuteQueryUnitFlowsForTable(const Protocol::UnitFlowsParams &requestParams,
        const std::pair<std::string, std::string> &tableAndSql, uint64_t minTimestamp, const std::string &connectionId,
        const std::vector<uint64_t> &deviceIdList);
    bool AssembleUnitFlowOfTypeMSTX(const std::vector<FlowLocation> &mstxFlowLocationList,
        const std::vector<FlowLocation> &taskFlowLocationList, const std::string &connectionId,
        Protocol::UnitFlowsBody &responseBody);
    bool AssembleUnitFlowOfTypePyTorchToCANNToAscendHardwareToCommunication(
        const std::vector<FlowLocation> &pytorchFlowLocationList, const std::vector<FlowLocation> &cannFlowLocationList,
        const std::vector<FlowLocation> &taskFlowLocationList,
        const std::vector<FlowLocation> &communicationOpFlowLocationList, const std::string &connectionId,
        Protocol::UnitFlowsBody &responseBody);
    bool AssembleUnitFlowOfTypeAsyncTaskQueue(const std::vector<FlowLocation> &pytorchFlowLocationList,
        const std::string &connectionId, Protocol::UnitFlowsBody &responseBody);
    bool AssembleUnitFlowOfTypeFwdBwd(const std::vector<FlowLocation> &pytorchFlowLocationList,
        const std::string &connectionId, Protocol::UnitFlowsBody &responseBody);

    uint32_t SearchSliceNameCount(const Protocol::SearchCountParams &params);

    bool SearchSliceName(const Protocol::SearchSliceParams &params, int index, uint64_t minTimestamp,
        Protocol::SearchSliceBody &responseBody);

    bool SearchAllSlicesDetails(
        const Protocol::SearchAllSliceParams &params, Protocol::SearchAllSlicesBody &body, uint64_t minTimestamp);
    void AddColumns2Table(
        const bool isExist, const std::string &tableName, const std::string &columnName, const std::string &columnType);
    static std::map<std::string, std::map<std::string, std::string>> stringsCache;
    static std::string GetSingleSearchCountLockRangeSql(const SearchCountParams &params, const TrackQuery &item);
    static std::string GetKernelDetailFilterSqlWithHCCL(const KernelDetailsParams &requestParams);

    static std::string GetKernelDetailFilterSqlWithoutHCCL(const KernelDetailsParams &requestParams);

    static std::string GetKernelDetailSqlWithHCCL(
        const KernelDetailsParams &requestParams, const std::string &blockNumColumnName);

    static std::string GetKernelDetailSqlWithoutHCCL(
        const KernelDetailsParams &requestParams, const std::string &blockNumColumnName);

    static void UpdateAscendHardwareFlowLocationName(
        const std::string &rankId, std::vector<FlowLocation> &flowLocations);
    bool FillDictMap(
        LightSliceCache& cache, const Protocol::SearchAllSliceParams& params, std::unordered_set<int32_t>& matchedIds);
    void LoadTableData(LightSliceCache& cache, const std::unordered_set<int32_t>& matchedIds,
        bool isExist, const std::string& sql, SliceTableType tableType);

    static std::string GetSliceDetailSql(SliceTableType type, uint64_t minTimestamp, const std::string& idList);
    static std::string BuildIdList(const std::vector<uint64_t>& ids);
    void FillSearchAllSlices(const LightSliceCache& cache, const Protocol::SearchAllSliceParams& params,
        SqliteResultSet* result, Protocol::SearchAllSlicesBody& body);
};
}
// clang-format on

#endif // PROFILER_SERVER_DBTRACEDATABASE_H
