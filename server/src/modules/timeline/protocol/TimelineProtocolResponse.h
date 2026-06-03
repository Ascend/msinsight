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

#ifndef DIC_TIMELINE_PROTOCOL_RESPONSE_H
#define DIC_TIMELINE_PROTOCOL_RESPONSE_H

#include <utility>
#include <vector>
#include <set>
#include <cmath>
#include <atomic>
#include <unordered_map>
#include "GlobalDefs.h"
#include "ProtocolDefs.h"
#include "ProtocolParamUtil.h"
#include "ProtocolMessage.h"
#include "NumberUtil.h"
#include "TimelineBodyStruct.h"
#include "SystemMemoryDatabase.h"

namespace Dic {
namespace Protocol {
using namespace Dic::Module::Global;
struct Action {
    std::string cardName;
    std::string rankId;
    std::string cardPath;
    std::string host;
    std::string fileId;
    std::string cluster;
    bool result = true;
    int64_t projectType;
    std::vector<std::string> dataPathList;
};

struct ImportActionResBody {
    std::vector<Action> result;
    std::vector<std::string> coreList;
    std::vector<std::string> sourceList;
    std::vector<std::shared_ptr<ParseFileInfo>> projectFileTree;
    bool isBinary = false;
    bool isCluster = false;
    bool isOnlyTraceJson = false;
    bool reset = true;
    bool isSimulation = false;
    bool isIpynb = false;
    bool isLeaks = false;
    // 导入文件是否待解析
    bool isPending = false;
    bool hasCachelineRecords = false;
    uint16_t version = 0x5a;
    bool isIE = false;
    bool isMultiDevice{false};
    bool isTriton{false};
    bool isFtrace{false};
};

struct ImportActionResponse : public Response {
    ImportActionResponse() : Response(REQ_RES_IMPORT_ACTION) {}
    ImportActionResBody body;
};

struct ThreadTraces {
    std::string name;
    uint64_t duration = 0;
    uint64_t startTime = 0;
    uint64_t endTime = 0;
    uint32_t depth = 0;
    std::string threadId;
    std::string pid;
    std::string id;
    std::string cname;
    std::string opConnectionId;
};

struct UnitThreadTracesBody {
    uint64_t maxDepth = 0;
    uint64_t currentMaxDepth = 0;
    bool havePythonFunction = false;
    bool isLoading = false;
    std::vector<std::vector<ThreadTraces>> data;
};

struct UnitThreadTracesResponse : public Response {
    UnitThreadTracesResponse() : Response(REQ_RES_UNIT_THREAD_TRACES) {}
    UnitThreadTracesBody body;
};

struct CreateCurveBody {
    std::string curveName;
};

struct CreateCurveResponse : public Response {
    CreateCurveResponse() : Response(REQ_RES_CREATE_CURVE) {}
    CreateCurveBody body;
};

struct ThreadTracesSummary {
    uint64_t startTime = 0;
    uint64_t duration = 0;
};

struct UnitThreadTracesSummaryBody {
    std::vector<ThreadTracesSummary> data;
};

struct UnitThreadTracesSummaryResponse : public Response {
    UnitThreadTracesSummaryResponse() : Response(REQ_RES_UNIT_THREAD_TRACES) {}
    UnitThreadTracesSummaryBody body;
};

struct SliceGroupItem {
    std::string title;
    uint64_t wallDuration = 0;
    uint64_t occurrences = 0;
    uint64_t avgWallDuration = 0;
    uint64_t maxWallDuration = 0;
    uint64_t minWallDuration = 0;
    uint64_t selfTime = 0;
    std::map<std::string, std::set<std::string>> processMap;
    std::set<std::string> metaTypeList;
};

struct UnitThreadsBody {
    bool emptyFlag = false;
    std::vector<SliceGroupItem> data;
};

struct UnitThreadsResponse : public Response {
    UnitThreadsResponse() : Response(REQ_RES_UNIT_THREADS) {}
    UnitThreadsBody body;
};

struct UnitThreadDetailBody {
    bool emptyFlag = false;
    ThreadDetail data;
};

struct UnitThreadDetailResponse : public Response {
    UnitThreadDetailResponse() : Response(REQ_RES_UNIT_THREAD_DETAIL) {}
    UnitThreadDetailBody body;
};

struct FlowLocation {
    std::string tid;
    std::string id;
    std::string metaType;
    std::string rankId;
    uint32_t depth = 0;
    uint64_t timestamp = 0;
    uint64_t duration = 0; // slice duration
    std::string pid;
    std::string name; // slice name
    std::string deviceId;
    std::string type;
    std::string note;
};

struct UnitSingleFlow {
    std::string cat;
    std::string id;
    FlowLocation from;
    FlowLocation to;
    std::string title;
};

struct UnitCatFlows {
    std::string cat;
    std::vector<UnitSingleFlow> flows;
};

struct UnitFlowsBody {
    std::vector<UnitCatFlows> unitAllFlows;
};

struct UnitFlowsResponse : public Response {
    UnitFlowsResponse() : Response(REQ_RES_UNIT_FLOWS) {}
    UnitFlowsBody body;
};

struct SetCardAliasBody {};

struct SetCardAliasResponse : public Response {
    SetCardAliasResponse() : Response(REQ_RES_UNIT_SET_CARD_ALIAS) {};
    SetCardAliasBody body;
};

struct ResetWindowResponse : public Response {
    ResetWindowResponse() : Response(REQ_RES_RESET_WINDOW) {}
};

inline bool CompareLess(uint32_t depth1, uint32_t depth2, uint64_t time1, uint64_t time2) {
    if (depth1 < depth2) {
        return true;
    }
    if (depth1 == depth2 && time1 < time2) {
        return true;
    }
    return false;
}

// struct
struct RowThreadTrace {
    int64_t id = 0;
    uint64_t startTime = 0;
    uint64_t duration = 0;
    uint32_t depth = 0;
    int64_t traceId = 0;
    std::string name;
    std::string cname;
    bool operator<(const RowThreadTrace &right) const {
        return CompareLess(depth, right.depth, startTime, right.startTime);
    }
};

struct ExtremumTimestamp {
    uint64_t minTimestamp = 0;
    uint64_t maxTimestamp = 0;
};

struct SimpleSlice {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    uint64_t duration = 0;
    uint64_t endTime = 0;
    uint32_t depth = 0;
    std::string name;
    std::string tid;
    std::string pid;
    std::string metaType;
    bool operator<(const SimpleSlice &right) const {
        return CompareLess(depth, right.depth, timestamp, right.timestamp);
    }

    bool operator>(const SimpleSlice &right) const {
        if (depth > right.depth) {
            return true;
        }
        if (depth == right.depth && timestamp > right.timestamp) {
            return true;
        }
        return false;
    }
};
struct SearchResult {
    std::string rankId;
    std::string dbPath;
    uint32_t count = 0;
};

struct SearchCountBody {
    uint32_t totalCount = 0;
    std::vector<SearchResult> countList;
};

struct SearchCountResponse : public Response {
    SearchCountResponse() : Response(REQ_RES_SEARCH_COUNT) {}
    SearchCountBody body;
};

struct SearchSliceBody {
    std::string rankId;
    std::string pid;
    std::string tid;
    std::string id;
    std::string dbPath;
    uint64_t startTime = 0;
    uint64_t duration = 0;
    uint32_t depth = 0;
    std::string metaType;
};

struct SearchSliceResponse : public Response {
    SearchSliceResponse() : Response(REQ_RES_SEARCH_SLICE) {}
    SearchSliceBody body;
};

struct RemoteDeleteBody {
    bool startTimeUpdated = false;
    uint64_t maxTimeStamp = 0;
};

struct RemoteDeleteResponse : public Response {
    RemoteDeleteResponse() : Response(REQ_RES_REMOTE_DELETE) {}
    RemoteDeleteBody body;
};

struct FlowCategoryListBody {
    std::vector<std::string> category;
};

struct FlowCategoryListResponse : public Response {
    FlowCategoryListResponse() : Response(REQ_RES_FLOW_CATEGORY_LIST) {}
    FlowCategoryListBody body;
};

struct FlowCategoryEventsBody {
    std::vector<std::unique_ptr<UnitSingleFlow>> flowDetailList;
};

struct FlowCategoryEventsResponse : public Response {
    FlowCategoryEventsResponse() : Response(REQ_RES_FLOW_CATEGORY_EVENTS) {}
    FlowCategoryEventsBody body;
};

struct UnitCounterData {
    uint64_t timestamp = 0;
    std::string valueJsonStr; // json string, need to convert to json object before send
};

struct UnitCounterBody {
    std::vector<UnitCounterData> data;
};

struct UnitCounterResponse : public Response {
    UnitCounterResponse() : Response(REQ_RES_UNIT_COUNTER) {}
    UnitCounterBody body;
};

struct SystemViewDetail {
    std::string name;
    double time;
    double totalTime;
    uint64_t numberCalls;
    double avg;
    double stdDev;
    double min;
    double max;
};

struct SystemViewBody {
    std::vector<SystemViewDetail> systemViewDetail;
    uint64_t total = 0;
    uint64_t pageSize{};
    uint64_t currentPage{};
};

struct SystemViewResponse : public Response {
    SystemViewResponse() : Response(REQ_RES_UNIT_SYSTEM_VIEW) {}
    SystemViewBody body;
};

struct SystemViewAICoreFreqDetail {
    double frequency; // 频率
    double timestamp; // 时间戳
};

struct ExpAnaAICoreFreqBody {
    std::string rankId;
    std::string pid;
    std::string tid;
    bool hasProblem = false;
    uint64_t percent = 0;
};

struct ExpAnaAICoreFreqResponse : public Response {
    ExpAnaAICoreFreqResponse() : Response(REQ_RES_EXPERT_ANALYSIS_AICORE_FREQ) {}
    ExpAnaAICoreFreqBody body;
};

struct SystemViewOverallRes {
    double totalTime{};
    double ratio{};
    uint32_t nums{};
    double avg{};
    double max = -std::numeric_limits<double>::infinity();
    double min = std::numeric_limits<double>::infinity();
    std::string name;
    std::vector<SystemViewOverallRes> children;
    uint32_t level{1};
    std::string id;

    void ValidateValues() {
        max = (max == -std::numeric_limits<double>::infinity()) ? 0.0 : max;
        min = (min == std::numeric_limits<double>::infinity()) ? 0.0 : min;
    }
};

struct CommunicationSummaryInfoByThread {
    std::string threadName;
    std::string group;
    std::string plane;
    uint64_t completeWaitTime{};
    uint64_t completeTransmitTime{};
    uint64_t uncoveredWaitTime{};
    uint64_t uncoveredTransmitTime{};

    void UpdateData(bool waitFlag, uint64_t completeTime, uint64_t uncoveredTime) {
        if (waitFlag) {
            completeWaitTime = completeWaitTime > UINT64_MAX - completeTime ? 0 : completeWaitTime + completeTime;
            uncoveredWaitTime = uncoveredWaitTime > UINT64_MAX - uncoveredTime ? 0 : uncoveredWaitTime + uncoveredTime;
        } else {
            completeTransmitTime =
                completeTransmitTime > UINT64_MAX - completeTime ? 0 : completeTransmitTime + completeTime;
            uncoveredTransmitTime =
                uncoveredTransmitTime > UINT64_MAX - uncoveredTime ? 0 : uncoveredTransmitTime + uncoveredTime;
        }
    }
};

struct CommunicationSummaryInfoByGroup {
    std::string groupName;
    CommunicationSummaryInfoByThread op;
    std::unordered_map<std::string, CommunicationSummaryInfoByThread> taskMap;
};

struct SystemViewOverallResponse : public Response {
    SystemViewOverallResponse() : Response(REQ_RES_SYSTEM_VIEW_OVERALL) {}
    std::vector<SystemViewOverallRes> details;
    PageParam pageParam;
    bool isLoading = false;
};

struct SystemViewFtraceStatResponse : public Response {
    SystemViewFtraceStatResponse() : Response(REQ_RES_SYSTEM_VIEW_FTRACE_STAT) {}
    std::vector<std::unordered_map<std::string, std::string>> data; // 查询结果数据
    PageParam pageParam;
};

class EventDetail {
  public:
    virtual ~EventDetail() = default;
    virtual void Base() {}
    std::string id;
    std::string name;
    uint64_t startTime = 0;
    uint64_t duration = 0;
    uint64_t depth = 0;
    std::string threadId;
    std::string processId;
};

class HostEventDetail : public EventDetail {
  public:
    std::string tid;
    std::string pid;
};

class DeviceEventDetail : public EventDetail {
  public:
    std::string threadName;
    std::string rankId;
};

struct EventsViewColumnAttr {
    std::string name;
    std::string type;
    std::string key;
};

struct EventsViewBody {
    std::vector<EventsViewColumnAttr> columnList;
    std::vector<std::unique_ptr<EventDetail>> eventDetailList;
    uint64_t count{};
    uint64_t pageSize{};
    uint64_t currentPage{};
};

struct EventsViewResponse : public Response {
    EventsViewResponse() : Response(REQ_RES_UNIT_EVENTS_VIEW) {}
    EventsViewBody body;
};

struct ParseCardsBody {
    bool isContinueParse = false;
};

struct ParseCardsResponse : public Response {
    ParseCardsResponse() : Response(REQ_RES_PARSE_CARDS) {}
    ParseCardsBody body;
};

struct KernelDetail {
    std::string id;
    std::string taskId;
    std::string name;
    std::string type;
    std::string acceleratorCore;
    uint64_t startTime;
    double duration;
    double waitTime;
    uint64_t blockNum;
    std::string inputShapes;
    std::string inputDataTypes;
    std::string inputFormats;
    std::string outputShapes;
    std::string outputDataTypes;
    std::string outputFormats;
};

struct KernelDetailsBody {
    std::vector<std::string> acceleratorCoreList;
    std::vector<KernelDetail> kernelDetails;
    uint64_t count = 0;
    uint64_t pageSize{};
    uint64_t currentPage{};
};

struct KernelDetailsResponse : public Response {
    KernelDetailsResponse() : Response(REQ_RES_UNIT_KERNEL_DETAILS) {}
    KernelDetailsBody body;
};

// Minimal Timeline slice locator for frontend highlight. Missing nodes are represented by missingReason.
struct KernelE2EHighlightSliceDto {
    std::string role;
    std::string name;
    uint64_t startTime = 0;
    uint64_t duration = 0;
    std::string pid;
    std::string tid;
    std::string id;
    std::string missingReason;
};

struct KernelE2ETimeRecordDto {
    std::string id;
    std::string opName;
    std::string pathType;
    bool isParent = false;
    std::optional<uint64_t> prepareTime;
    std::optional<uint64_t> pythonApiTime;
    std::optional<uint64_t> enqueueTime;
    std::optional<uint64_t> queueTime;
    std::optional<uint64_t> pipeline2Time;
    std::optional<uint64_t> launchTime;
    std::optional<uint64_t> endToEndTime;
    std::string status;
    std::string diagnostic;
    std::vector<KernelE2EHighlightSliceDto> highlightSlices;
    std::vector<KernelE2ETimeRecordDto> children;
};

struct KernelE2ETimeBody {
    std::vector<KernelE2ETimeRecordDto> records;
    uint64_t totalCount = 0;
    uint64_t normalCount = 0;
    uint64_t fallbackCount = 0;
    uint64_t incompleteCount = 0;
    double launchMatchRate = 0.0;
};

struct KernelE2ETimeResponse : public Response {
    KernelE2ETimeResponse() : Response(REQ_RES_KERNEL_E2E_TIME) {}
    KernelE2ETimeBody body;
};

struct OneKernelBody {
    std::string id;
    uint64_t depth = {0};
    uint64_t duration = 0;
    std::string threadId;
    std::string pid;
    std::string step;
    std::string group;
    std::string rankId;
};

struct KernelBaseInfo {
    std::string id;
    std::string rankId;
    std::string name;
    std::string type;
    uint64_t startTime{};
    uint64_t duration{};
    std::string pid;
    std::string tid;
    uint64_t depth{};
    std::string inputType;
    std::string outputType;
    std::string note;
};

struct OneKernelResponse : public Response {
    OneKernelResponse() : Response(REQ_RES_ONE_KERNEL_DETAILS) {}
    OneKernelBody body;
};

struct CommunicationKernelBody : public OneKernelBody {
    uint64_t startTime = 0;
};

struct CommunicationKernelResponse : public Response {
    CommunicationKernelResponse() : Response(REQ_RES_COMMUNICATION_KERNEL_DETAIL) {}
    CommunicationKernelBody body;
};

struct UnitThreadsOperatorsResponse : public Response {
    UnitThreadsOperatorsResponse() : Response(REQ_RES_SAME_OPERATORS_DURATION) {}
    UnitThreadsOperatorsBody body;
};

struct SearchAllSlicesResponse : public Response {
    SearchAllSlicesResponse() : Response(REQ_RES_SEARCH_ALL_SLICES) {}
    SearchAllSlicesBody body;
};

struct TableDataNameListResponse : public Response {
    TableDataNameListResponse() : Response(REQ_RES_TABLE_DATA_NAME_LIST) {}
    TableDataListBody body;
};

struct TableDataDetailResponse : public Response {
    TableDataDetailResponse() : Response(REQ_RES_TABLE_DATA_DETAIL) {}
    TableDataDatail body;
};

struct MemcpyOverallRes {
    std::string key; // 唯一标识符
    std::string name; // 显示名称 (threadId 或 memcpy type)
    uint64_t totalSize{}; // 总大小
    double totalTime{}; // 总时间
    uint32_t number{}; // 数量
    double avgSize{}; // 平均大小
    uint64_t minSize{}; // 最小大小
    uint64_t maxSize{}; // 最大大小
    double avgTime{}; // 平均时间
    double minTime{}; // 最小时间
    double maxTime{}; // 最大时间
    int level{}; // 层级 (1: thread, 2: memcpy type)
    std::vector<MemcpyOverallRes> children; // 子项 (memcpy types for thread)
};

struct MemcpyOverallResponse : public Response {
    std::vector<MemcpyOverallRes> details;
    PageParam pageParam;
    bool isLoading = false;
};

struct MemcpyDetailResponse : public Response {
    MemcpyDetailResponse() : Response(REQ_RES_MEMCPY_DETAIL) {}
    MemcpyDetailsBody body;
};

struct RankOffsetItem {
    std::string rankId;
    int64_t offset = 0;
    std::vector<std::string> processId;
};

struct RankOffsetError {
    int32_t code = 0;
    std::string message;
};

struct RankOffsetBody {
    std::vector<RankOffsetItem> successList;
    uint64_t baseOffset = 0;
    RankOffsetError error;
};

struct RankOffsetResponse : public Response {
    RankOffsetResponse() : Response(REQ_RES_RANK_OFFSET) {}
    RankOffsetBody body;
};
} // end of namespace Protocol
} // end of namespace Dic

#endif // DIC_TIMELINE_PROTOCOL_RESPONSE_H
