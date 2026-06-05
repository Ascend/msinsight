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

#ifndef DIC_PROTOCOL_DEFS_H
#define DIC_PROTOCOL_DEFS_H

#include <string>

namespace Dic {
namespace Protocol {
#pragma region <<JsonDataBase>>
const std::string LINE_START = "s";
const std::string LINE_END = "f";
const std::string LINE_END_OPTIONAL = "t";
#pragma region <<JsonDataBase>>

#pragma region <<Module>>
const std::string MODULE_GLOBAL = "global";
const std::string MODULE_TIMELINE = "timeline";
const std::string MODULE_SUMMARY = "summary";
const std::string MODULE_COMMUNICATION = "communication";
const std::string MODULE_MEMORY = "memory";
const std::string MODULE_OPERATOR = "operator";
const std::string MODULE_SOURCE = "source";
const std::string MODULE_RL = "rl";
const std::string MODULE_ADVISOR = "advisor";
const std::string MODULE_JUPYTER = "jupyter";
const std::string MODULE_IE = "IE";
const std::string MODULE_MEM_SCOPE = "leaks";
const std::string MODULE_TRITON = "triton";
const std::string MODULE_MEM_SNAPSHOT = "memsnapshot";
#pragma endregion

#pragma region << Event Name>>
const std::string EVENT_INITIALIZED = "initialized";
// timeline
const std::string EVENT_PARSE_SUCCESS = "parse/success";
const std::string EVENT_PARSE_FAIL = "parse/fail";
const std::string EVENT_PARSE_CLUSTER_COMPLETED = "parse/clusterCompleted";
const std::string EVENT_ALL_SUCCESS = "allPagesSuccess";
const std::string EVENT_PARSE_CLUSTER_STEP2_COMPLETED = "parse/clusterStep2Completed";
const std::string EVENT_PARSE_MEMORY_COMPLETED = "parse/memoryCompleted";
const std::string EVENT_PARSE_IE_COMPLETED = "parse/statisticCompleted";
const std::string EVENT_PARSE_MEM_SCOPE_COMPLETED = "parse/leaksMemoryCompleted";
const std::string EVENT_MODULE_RESET = "module.reset";
const std::string EVENT_PARSE_PROGRESS = "parse/progress";
const std::string EVENT_PARSE_HEATMAP_COMPLETED = "parse/heatmapCompleted";
const std::string EVENT_PARSE_UNIT_COMPLETED = "parse/unitCompleted";
#pragma endregion

#pragma region << Request / Response Command>>
// global
const std::string REQ_RES_HEART_CHECK = "heartCheck";
const std::string REQ_RES_FILES_GET = "files/get";
const std::string REQ_RES_GET_MODULE_CONFIG = "moduleConfig/get";
const std::string REQ_RES_PROJECT_EXPLORER_UPDATE = "files/updateProjectExplorer";
const std::string REQ_RES_PROJECT_EXPLORER_INFO_GET = "files/getProjectExplorer";
const std::string REQ_RES_PROJECT_EXPLORER_INFO_DELETE = "files/deleteProjectExplorer";
const std::string REQ_RES_PROJECT_EXPLORER_CLEAR = "files/clearProjectExplorer";
const std::string REQ_RES_PROJECT_VALID_CHECK = "files/checkProjectValid";
const std::string REQ_RES_PROJECT_SET_BASELINE = "global/setBaseline";
const std::string REQ_RES_PROJECT_CANCEL_BASELINE = "global/cancelBaseline";
const std::string EVENT_FILES_READ_FAIL = "files/read/fail";

// timeline
const std::string REQ_RES_IMPORT_ACTION = "import/action";
const std::string REQ_RES_PARSE_CARDS = "parse/cards";
const std::string REQ_RES_UNIT_THREAD_TRACES = "unit/threadTraces";
const std::string REQ_RES_UNIT_THREAD_TRACES_SUMMARY = "unit/threadTracesSummary";
const std::string REQ_RES_UNIT_THREADS = "unit/threads";
const std::string REQ_RES_UNIT_THREAD_DETAIL = "unit/threadDetail";
const std::string REQ_RES_UNIT_FLOWS = "unit/flows";
const std::string REQ_RES_RESET_WINDOW = "remote/reset";
const std::string REQ_RES_UNIT_SET_CARD_ALIAS = "unit/setCardAlias";
const std::string REQ_RES_UNIT_SYSTEM_VIEW = "unit/systemView";
const std::string REQ_RES_UNIT_SYSTEM_VIEW_TRACE = "unit/systemViewTrace";
const std::string REQ_RES_UNIT_EVENTS_VIEW = "unit/eventView";
const std::string REQ_RES_UNIT_KERNEL_DETAILS = "unit/kernelDetails";
const std::string REQ_RES_KERNEL_E2E_TIME = "unit/kernelE2ETime";
const std::string REQ_RES_ONE_KERNEL_DETAILS = "unit/one/kernelDetail";
const std::string REQ_RES_COMMUNICATION_KERNEL_DETAIL = "unit/kernelDetail";
const std::string REQ_RES_SEARCH_COUNT = "search/count";
const std::string REQ_RES_SEARCH_SLICE = "search/slice";
const std::string REQ_RES_SEARCH_ALL_SLICES = "search/all/slices";
const std::string REQ_RES_REMOTE_DELETE = "remote/delete";
const std::string REQ_RES_FLOW_CATEGORY_LIST = "flow/categoryList";
const std::string REQ_RES_FLOW_CATEGORY_EVENTS = "flow/categoryEvents";
const std::string REQ_RES_UNIT_COUNTER = "unit/counter";
const std::string REQ_RES_CREATE_CURVE = "create/curve";
const std::string REQ_RES_SAME_OPERATORS_DURATION = "query/all/same/operators/duration";
const std::string REQ_RES_SYSTEM_VIEW_OVERALL = "systemView/overall";
const std::string REQ_RES_SYSTEM_VIEW_OVERALL_MORE_DETAILS = "systemView/overall/more/details";
const std::string REQ_RES_EXPERT_ANALYSIS_AICORE_FREQ = "expertAnalysis/AICoreFreq";
const std::string REQ_RES_SYSTEM_VIEW_FTRACE_STAT = "systemView/systemViewFtraceStat";
const std::string REQ_RES_TABLE_DATA_NAME_LIST = "tableData/nameList";
const std::string REQ_RES_TABLE_DATA_DETAIL = "tableData/detail";
const std::string REQ_RES_MEMCPY_OVERALL = "memcpy/total/list";
const std::string REQ_RES_MEMCPY_DETAIL = "memcpy/detail/list";
const std::string REQ_RES_RANK_OFFSET = "timeline/rankOffset";

// communication
const std::string REQ_RES_COMMUNICATION_OPERATOR_DETAILS = "communication/operatorDetails";
const std::string REQ_RES_COMMUNICATION_OPERATOR_LISTS = "communication/operatorLists";
const std::string REQ_RES_COMMUNICATION_BANDWIDTH = "communication/bandwidth";
const std::string REQ_RES_COMMUNICATION_DISTRIBUTION = "communication/distribution";
const std::string REQ_RES_COMMUNICATION_ITERATIONS = "communication/duration/iterations";
const std::string REQ_RES_COMMUNICATION_OPERATORNAMES = "communication/duration/operatorNames";
const std::string REQ_RES_COMMUNICATION_SORT_OP = "communication/matrix/sortOpNames";
const std::string REQ_RES_COMMUNICATION_LIST = "communication/duration/list";
const std::string REQ_RES_COMMUNICATION_ADVISOR = "communication/advisor";
const std::string REQ_RES_COMMUNICATION_DURATION_SLOW_RANK_LIST = "communication/duration/slow-rank/list";

// summary
const std::string REQ_RES_SUMMARY_QUERY_TOP_DATA = "summary/queryTopData";
const std::string REQ_RES_PARALLELISM_ARRANGEMENT_ALL = "parallelism/arrangement/all";
const std::string REQ_RES_PARALLELISM_PERFORMANCE_DATA = "parallelism/performance/data";
const std::string REQ_RES_SUMMARY_STATISTIC = "summary/statistic";
const std::string REQ_RES_SUMMARY_QUERY_PARALLEL_STRATEGY = "summary/query/parallelStrategy";
const std::string REQ_RES_SUMMARY_SET_PARALLEL_STRATEGY = "summary/set/parallelStrategy";
const std::string REQ_RES_COMPUTE_DETAIL = "summary/queryComputeDetail";
const std::string REQ_RES_COMMUNICATION_DETAIL = "summary/queryCommunicationDetail";
const std::string REQ_RES_IMPORT_EXPERT_DATA = "summary/importExpertData";
const std::string REQ_RES_QUERY_EXPERT_HOTSPOT = "summary/queryExpertHotspot";
const std::string REQ_RES_QUERY_MODEL_INFO = "summary/queryModelInfo";
const std::string REQ_RES_SUMMARY_SLOW_RANK_ADVISOR = "summary/slowRank/advisor";

// pipeline
const std::string REQ_RES_PIPELINE_FWD_BWD_TIMELINE = "parallelism/pipeline/fwdBwdTimeline";

// matrix
const std::string REQ_RES_COMMUNICATION_MATRIX_GROUP = "communication/matrix/group";
const std::string REQ_RES_COMMUNICATION_MATRIX_BANDWIDTH = "communication/matrix/bandwidthInfo";

// memory
const std::string REQ_RES_MEMORY_TYPE = "Memory/view/type";
const std::string REQ_RES_MEMORY_RESOURCE_TYPE = "Memory/view/resourceType";
const std::string REQ_RES_MEMORY_OPERATOR = "Memory/view/operator";
const std::string REQ_RES_MEMORY_COMPONENT = "Memory/view/component";
const std::string REQ_RES_MEMORY_VIEW = "Memory/view/memoryUsage";
const std::string REQ_RES_MEMORY_OPERATOR_MIN_MAX = "Memory/view/operatorSize";
const std::string REQ_RES_MEMORY_STATIC_OP_MEMORY_GRAPH = "Memory/view/staticOpMemoryGraph";
const std::string REQ_RES_MEMORY_STATIC_OP_MEMORY_LIST = "Memory/view/staticOpMemoryList";
const std::string REQ_RES_MEMORY_STATIC_OP_MEMORY_MIN_MAX = "Memory/view/staticOpMemorySize";
const std::string REQ_RES_MEMORY_FIND_SLICE = "Memory/find/slice";

// memscope Request/Response
const std::string REQ_RES_MEM_SCOPE_MEMORY_BLOCKS = "Memory/leaks/blocks";
const std::string REQ_RES_MEM_SCOPE_MEMORY_ALLOCATIONS = "Memory/leaks/allocations";
const std::string REQ_RES_MEM_SCOPE_MEMORY_DETAILS = "Memory/leaks/details";
const std::string REQ_RES_MEM_SCOPE_PYTHON_TRACES = "Memory/leaks/traces";
const std::string REQ_RES_MEM_SCOPE_EVENTS = "Memory/leaks/events";

// triton Request/Response
const std::string REQ_RES_TRITON_MEMORY_BLOCKS = "Triton/memory/blocks";
const std::string REQ_RES_TRITON_MEMORY_BASIC_INFO = "Triton/memory/basicInfo";
const std::string REQ_RES_TRITON_MEMORY_USAGE = "Triton/memory/usage";

// memsnapshot Request
const std::string REQ_RES_MEM_SNAPSHOT_BLOCKS = "Memory/snapshot/blocks";
const std::string REQ_RES_MEM_SNAPSHOT_ALLOCATIONS = "Memory/snapshot/allocations";
const std::string REQ_RES_MEM_SNAPSHOT_EVENTS = "Memory/snapshot/events";
const std::string REQ_RES_MEM_SNAPSHOT_DETAIL = "Memory/snapshot/detail";
const std::string REQ_RES_MEM_SNAPSHOT_STATE = "Memory/snapshot/state";
const std::string REQ_RES_MEM_SNAPSHOT_LEAK_STATS = "Memory/snapshot/leakStats";
const std::string EVENT_PARSE_MEM_SNAPSHOT_PROGRESS = "parse/memSnapshotProgress";

// Operator Request
const std::string REQ_RES_OPERATOR_CATEGORY_INFO = "operator/category";
const std::string REQ_RES_OPERATOR_COMPUTE_UNIT_INFO = "operator/compute_unit";
const std::string REQ_RES_OPERATOR_STATISTIC_INFO = "operator/statistic";
const std::string REQ_RES_OPERATOR_DETAIL_INFO = "operator/details";
const std::string REQ_RES_OPERATOR_MORE_INFO = "operator/more_info";
const std::string REQ_RES_OPERATOR_EXPORT_DETAILS = "operator/exportDetails";

// Operator Event
const std::string EVENT_PARSE_OPERATOR_STATUS = "parse/operatorCompleted";
const std::string EVENT_PARSE_OPERATOR_CLEAR = "parse/operatorClear";

// Source Request
const std::string REQ_RES_SOURCE_CODE_FILE = "source/code/file";
const std::string REQ_RES_SOURCE_API_LINE = "source/api/line";
const std::string REQ_RES_SOURCE_API_LINE_DYNAMIC = "source/api/line/dynamic";
const std::string REQ_RES_SOURCE_API_INSTRUCTIONS = "source/api/instructions";
const std::string REQ_RES_SOURCE_API_INSTRUCTIONS_DYNAMIC = "source/api/instructions/dynamic";
const std::string REQ_RES_DETAILS_BASE_INFO = "source/details/baseInfo";
const std::string REQ_RES_DETAILS_COMPUTE_LOAD_INFO = "source/details/computeworkload";
const std::string REQ_RES_DETAILS_COMPUTE_MEMORY_GRAPH = "source/details/memoryGraph";
const std::string REQ_RES_DETAILS_COMPUTE_MEMORY_TABLE = "source/details/memoryTable";
const std::string REQ_RES_DETAILS_INTER_CORE_LOAD_GRAPH = "source/details/interCoreLoadAnalysis";
constexpr inline std::string_view REQ_RES_DETAILS_ROOFLINE = "source/details/roofline";
constexpr std::string_view REQ_RES_CACHELINE_RECORD = "source/cache/cachelineRecords";

// Advisor Request/Response
const std::string REQ_RES_ADVISOR_AFFINITY_OPTIMIZER = "advisor/affinity_optimizer";
const std::string REQ_RES_ADVISOR_AFFINITY_API = "advisor/affinity_api";
const std::string REQ_RES_ADVISOR_OPERATORS_FUSION = "advisor/operator_fusion";
const std::string REQ_RES_ADVISOR_AICPU_OPERATORS = "advisor/aicpu_operator";
const std::string REQ_RES_ADVISOR_ACLNN_OPERATORS = "advisor/aclnn_operator";
const std::string REQ_RES_ADVISOR_OPERATOR_DISPATCH = "advisor/operatorDispatch";

// IE
const std::string REQ_RES_IE_VIEW = "IE/usage/view";
const std::string REQ_RES_IE_TABLE_VIEW = "IE/table/view";
const std::string REQ_RES_IE_DATA_GROUP = "IE/group";

// Reinforcement Learning 强化学习
const std::string REQ_REQ_RL_PIPELINE = "RL/pipeline";

const std::string KEY_BODY = "body";
#pragma endregion

template <typename T> struct CompareData {
    T baseline;
    T compare;
    T diff;
};

#if defined(__linux__) || defined(__APPLE__)
const std::string FILE_DESCRIPTOR_RUN_OUT_MESSAGE =
    ". The reason could be that file descriptors of profiler_server have run out."
    " Use \"ulimit -a\" to see the soft limit of file descriptors."
    " Use \"lsof -p pid | wc -l\" to see the current file descriptors of profiler_server."
    " If file descriptors run out, you could use \"ulimit -n number\" to set the soft limit"
    " of file descriptors and restart MindStudio Insight in the terminal to solve the problem.";
#endif
} // end of namespace
}

#endif // DIC_PROTOCOL_DEFS_H
