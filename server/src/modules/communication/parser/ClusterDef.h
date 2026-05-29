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

#ifndef PROFILER_SERVER_CLUSTER_DEF_H
#define PROFILER_SERVER_CLUSTER_DEF_H

#include <string>
#include <map>
#include <optional>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include "ServerLog.h"

namespace Dic {
namespace Module {
struct CommunicationTimeInfo {
    std::string iterationId;
    std::string stageId;
    std::string rankId;
    std::string opName;
    std::string opSuffix;
    uint64_t startTime = 0;
    double elapseTime = 0;
    double synchronizationTimeRatio = 0;
    double synchronizationTime = 0;
    double transitTime = 0;
    double waitTimeRatio = 0;
    double waitTime = 0;
    double idleTime = 0;
};

struct PackageInfo {
    double packageNumber;
    double packageTime;
};
struct CommunicationBandWidth {
    std::string iterationId;
    std::string rankId;
    std::string stageId;
    std::string opName;
    std::string opSuffix;
    std::string transportType;
    double bandwidthSize = 0;
    double bandwidthUtilization = 0;
    double largePackageRatio = 0;
    std::string sizeDistribution;
    double transitSize = 0;
    double transitTime = 0;
    // the key is the size of package
    std::unordered_map<std::string, PackageInfo> packageMap;
};
struct StepStatistic {
    std::string rankId;
    std::string stepId;
    std::string stageId;
    double computingTime = 0;
    double pureCommunicationTime = 0;
    double overlapCommunicationTime = 0;
    double communicationTime = 0;
    double freeTime = 0;
    double stageTime = 0;
    double bubbleTime = 0;
    double pureCommunicationExcludeReceiveTime = 0;
    double prepareTime = -1;
    double npuTotalTime = 0;
    int64_t dpIndex = 0;
    int64_t ppIndex = 0;
    int64_t tpIndex = 0;
};

const std::string PARALLEL_CONFIG_LEVEL_COLLECTED = "collected";
const std::string PARALLEL_CONFIG_LEVEL_PREDICTED = "predicted";
const std::string PARALLEL_CONFIG_LEVEL_CONFIRMED = "confirmed";
const std::string PARALLEL_CONFIG_LEVEL_CONFIGURED = "configured";
const std::string PARALLEL_CONFIG_LEVEL_UNDEFINED = "undefined";

// 并行策略算法类
const std::string MEGATRON_ALG = "megatron";
const std::string MINDSPEED_ALG = "mindspeed";
const std::string MINDIE_LLM_ALG = "mindie-llm";

/**
 * 1.TP+CP+DP+EP+PP (EP cross DP)
 * 此排布方式来自于Megatron-Core，排布的顺序是TP->CP->DP->PP，EP横跨DP之上且不参与或不影响排布编号（即要求DP能够被EP整除）
 */
const std::string MEGATRON_LM_TP_CP_EP_DP_PP_ALG = "megatron-lm(tp-cp-ep-dp-pp)";

/**
 * 2.TP+CP+PP+DP+EP (EP cross DP)
 * 此排布方式也来自于Megatron-Core，排布的顺序是TP->CP->PP->DP，EP横跨DP之上且不参与或不影响排布编号
 */
const std::string MEGATRON_LM_TP_CP_PP_EP_DP_ALG = "megatron-lm(tp-cp-pp-ep-dp)";

/**
 * 3.TP+CP+DP+EP+PP (EP cross CP and DP)
 * 此排布方式来自于MindSpeed-Core，排布顺序TP->CP->DP->PP（与1类似），EP横跨CP+DP之上且不参与或不影响排布编号（即要求CP*DP能够被EP整除)
 */
const std::string MINDSPEED_TP_CP_EP_DP_PP_ALG = "mindspeed(tp-cp-ep-dp-pp)";

/**
 * 4.TP+DP+EP+PP+MOE_TP (EP independent)
 * 来自于MINDIE_LLM（DeepSeek V3也可以算作此类型）,非MOE层采用TP->DP->PP顺序排布，MOE层采用MOE_TP->EP顺序排布
 */
const std::string MINDIE_LLM_TP_DP_EP_PP_MOETP_ALG = "mindie-llm(tp-dp-ep-pp-moetp)";

/**
 * 5.TP+PP+DP+EP (EP cross TP and DP)
 * 此排布方式来自于vLLM, 排布的顺序是TP->PP->DP, EP横跨TP+DP之上且不影响排布编号（即要求TP*DP能够被EP整除)
 */
const std::string VLLM_TP_PP_DP_EP_ALG = "vllm(tp-pp-dp-ep)";

const std::vector<std::string> ALGORITHMS_ALLOWED = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, MEGATRON_LM_TP_CP_PP_EP_DP_ALG,
    MINDSPEED_TP_CP_EP_DP_PP_ALG, MINDIE_LLM_TP_DP_EP_PP_MOETP_ALG, VLLM_TP_PP_DP_EP_ALG};

// MindSpeed相关高级参数
const std::string MINDSPEED_ULYSSES_CP_ALG = "ulysses_cp_algo";
const std::string MINDSPEED_MEGATRON_CP_ALG = "megatron_cp_algo";
const std::string MINDSPEED_HYBIRD_CP_ALG = "hybird_cp_algo";
const std::string MINDSPEED_HYBIRD_ADAPTIVE_CP_ALG = "hybird_adaptive_cp_algo";
const std::vector<std::string> MINDSPEED_CP_ALGORITHM_ALLOWED = {
    MINDSPEED_ULYSSES_CP_ALG, MINDSPEED_MEGATRON_CP_ALG, MINDSPEED_HYBIRD_CP_ALG, MINDSPEED_HYBIRD_ADAPTIVE_CP_ALG};
// 并行策略层次化展开，域维度
const std::string DIMENSIONS_DP = "ep-dp";
const std::string DIMENSIONS_PP = "ep-dp-pp";
const std::string DIMENSIONS_CP = "ep-dp-pp-cp";
const std::string DIMENSIONS_TP = "ep-dp-pp-cp-tp";
const std::vector<std::string> DIMENSIONS_ALLOWED = {DIMENSIONS_DP, DIMENSIONS_PP, DIMENSIONS_CP, DIMENSIONS_TP};
const std::string PP_PARA = "pp";
const std::string CP_PARA = "cp";
const std::string DP_PARA = "dp";
const std::string TP_PARA = "tp";
const std::string EP_PARA = "ep";
const std::string MOE_TP_PARA = "moeTp";
// 并行策略坐标
const std::string STR_INDEX = "Index";
const std::string PP_INDEX = "ppIndex";
const std::string CP_INDEX = "cpIndex";
const std::string DP_INDEX = "dpIndex";
const std::string TP_INDEX = "tpIndex";
const std::string EP_INDEX = "epIndex";
const std::string MOE_TP_INDEX = "moeTpIndex";
const uint32_t MAX_PARALLEL_SIZE = 10000;
const uint64_t MAX_WORLD_SIZE = 1000000;

struct ParallelStrategyConfigForMindSpeed {
    bool useTp2D = false;
    uint32_t nd1dim1 = 1;
    uint32_t nd2dim1 = 1;
    std::string cpAlgo;
    uint32_t ulyssesDegree = 1;
    uint32_t winSize = 1;
};
bool operator==(const ParallelStrategyConfigForMindSpeed &lhs, const ParallelStrategyConfigForMindSpeed &rhs);

struct ParallelStrategyConfig {
    std::string algorithm = MEGATRON_LM_TP_CP_EP_DP_PP_ALG;
    uint32_t ppSize{};
    uint32_t tpSize{};
    uint32_t dpSize{};
    uint32_t cpSize = 1;
    uint32_t epSize = 1;
    uint32_t moeTpSize = 1;
    std::string clusterPath;
    ParallelStrategyConfigForMindSpeed configForMindSpeed;
    bool CheckParams(std::string &errorMsg) const;
    bool CheckBaseParams(std::string &errorMsg) const;
    bool CheckParamForMindSpeed(std::string &errorMsg) const;
    bool CheckWinSizeForMindSpeed(std::string &errorMsg) const;
    bool CheckTp2DSizeForMindSpeed(std::string &errorMsg) const;
    bool CheckParamForMegatron(std::string &errorMsg) const;
    bool CheckParamForMindIELLM(std::string &errorMsg) const;
    bool CheckParamForVLLM(std::string &errorMsg) const;
};
bool operator==(const ParallelStrategyConfig &lhs, const ParallelStrategyConfig &rhs);

struct ClusterBaseInfo {
    std::string filePath;
    std::string ranks;
    std::string steps;
    std::string ppStages;
    std::string stages;
    ParallelStrategyConfig config;
    std::string level;
};

struct CommunicationMatrixInfo {
    std::string groupId;
    std::string iterationId;
    std::string sortOp;
    std::string opName;
    std::string groupName;
    int srcRank;
    int dstRank;
    std::string transportType;
    double transitSize;
    double transitTime;
    double bandwidth;
};

// cluster_step_trace_time.csv表头
const std::string FIELD_STEP = "Step";
const std::string FIELD_TYPE = "Type";
const std::string FIELD_INDEX = "Index";
const std::string FIELD_COMPUTING = "Computing";
const std::string FIELD_COMMUNICATION = "Communication";
const std::string FIELD_FREE = "Free";
const std::string FIELD_COMMUNICATION_NOT_OVERLAPPED = "Communication(Not Overlapped)";
const std::string FIELD_COMMUNICATION_NOT_OVERLAPPED_AND_RECEIVE = "Communication(Not Overlapped and Exclude Receive)";
const std::string FIELD_OVERLAPPED = "Overlapped";
const std::string FIELD_PREPARE_TIME = "Preparing";

const std::string FIELD_STAGE = "Stage";
const std::string FIELD_BUBBLE = "Bubble";

const std::string FIELD_DP_INDEX = "DP Index";
const std::string FIELD_PP_INDEX = "PP Index";
const std::string FIELD_TP_INDEX = "TP Index";
const std::string FIELD_CP_INDEX = "CP Index";
const std::string FIELD_EP_INDEX = "EP Index";

const std::string VALUE_PREPARING_TIME = "Preparing";
const std::string VALUE_TOTAL_COMPUTING_TIME = "Computing";
const std::string VALUE_COMPUTING_NOT_OVERLAPPED = "Computing(Not Overlapped)";
const std::string VALUE_TOTAL_COMMUNICATION = "Communication";
const std::string VALUE_COMMUNICATION_OVERLAPPED = "Computing/Communication Overlapped";
const std::string VALUE_COMMUNICATION_NOT_OVERLAPPED = "Communication(Not Overlapped)";
const std::string VALUE_COMMUNICATION_NOT_OVERLAPPED_AND_RECEIVE = "Communication(Not Overlapped and Exclude Receive)";
const std::string VALUE_FREE_TIME = "Free";
const std::string VALUE_STAGE_TIME = "Stage";
const std::string VALUE_BUBBLE_TIME = "Bubble";
const std::string VALUE_NPU_TIME = "Computing + Communication(Not Overlapped) + Free";

const std::vector<std::string> VALID_STEP_STATISTICS_HEADERS = {FIELD_STEP, FIELD_TYPE, FIELD_INDEX, FIELD_COMPUTING,
    FIELD_COMMUNICATION_NOT_OVERLAPPED, FIELD_OVERLAPPED, FIELD_COMMUNICATION, FIELD_FREE, FIELD_STAGE, FIELD_BUBBLE,
    FIELD_COMMUNICATION_NOT_OVERLAPPED_AND_RECEIVE};
const std::vector<std::string> PARALLEL_STRATEGY_HEADERS = {FIELD_DP_INDEX, FIELD_PP_INDEX, FIELD_TP_INDEX};

struct Position {
    uint32_t x = 0;
    uint32_t y = 0;
    bool operator==(const Position &other) const { return x == other.x && y == other.y; }
};

struct IndicatorAttr {
    uint8_t number{}; // No.
    std::string key;
    // 性能指标名称
    std::string name;
    // 是否需要在2D排布图上按色域渲染
    bool renderHeatMap = false;
    // 是否需要在下方"计算/通信概览"图中显示
    bool renderChart = false;
    // 计算/通信概览 是否默认显示
    bool visible = false;
    // 下方绘图时图表类型，柱形图还是折线图
    std::string chart;
    // 如果是堆积柱形图，堆积分类
    std::string stack;
    // 数据类型，用于区分y轴
    std::string yAxisType;
};

struct Connection {
    // 界面上一条连线连接多个element
    std::vector<uint32_t> indexes{};
    // dp, pp, tp, cp...
    std::string type;
    // 界面上一条连线对应多个通信域
    std::vector<std::string> communicationGroups;
    Connection() = default;
    Connection(
        std::string type, const std::vector<uint32_t> &indexes, const std::vector<std::string> &communicationGroups)
        : indexes(indexes), type(std::move(type)), communicationGroups(communicationGroups) {}
    Connection(const Connection &other) = default;
    Connection &operator=(const Connection &other) {
        if (this == &other) {
            return *this;
        }
        indexes = other.indexes;
        type = other.type;
        communicationGroups = other.communicationGroups;
        return *this;
    }
};

struct CommInfoUnderRank {
    double commTime = 0;
    std::string rankId;
    std::string rankSet;
    std::string pgName;
    std::string groupIdHash;
};

struct AdviceInfoForSlowRank {
    uint32_t index{}; // rank or group index
    std::string name; // rank or group name
    std::unordered_map<std::string, uint32_t> indexAttributes; // {key: pgName, value: xpIndex}
    std::unordered_map<std::string, double> synchronizeTime; // <key: pgName, value: xpSynchronizeTime>
    std::unordered_map<std::string, double> maxCommTime; // 所在通信域最大通信时间 <key: pgName, value: xpMaxCommTime>
};

// 一张卡或一个分组的相关信息，包括序号、名称、位置、并行分组属性、包含的卡等信息
struct Element {
    uint32_t index; // rank or group index
    std::string name; // rank or group name
    Position position{}; // rank or group position in 2D arrangement
    std::unordered_map<std::string, uint32_t> indexAttributes; // {key: pgName, value: xpIndex}
    std::vector<uint32_t> ranks;
    std::string formattedRanks;
};
struct ElementRankDetails {
    uint32_t ppIndexMin = 0;
    uint32_t ppIndexMax = 0;
    uint32_t cpIndexMin = 0;
    uint32_t cpIndexMax = 0;
    uint32_t tpIndexMin = 0;
    uint32_t tpIndexMax = 0;
};

struct RankMap {
    std::string rankId;
    std::string dbPath;
};

struct ArrangementAndConnectionData {
    uint32_t size{};
    std::vector<IndicatorAttr> indicators;
    std::vector<Element> arrangements; // rank or group arrangement and performance data
    std::vector<Connection> connections; // connection between ranks or groups
    std::vector<RankMap> rankDbPathList;
};

struct IndicatorDataStruct {
    uint32_t index; // rank or group index
    std::unordered_map<std::string, double> indicators; // performance data
};

struct GetPerformanceIndicatorParam {
    std::string step;
    std::string dimension;
    ParallelStrategyConfig config;
};

struct CommGroupParallelInfo {
    uint64_t id;
    std::string type; // p2p、collective
    std::string groupIdHash; // groupId的hash值
    std::string groupId; // 全局唯一
    std::vector<std::string> rankSet;
    std::string rankSetStr;
    std::string pgName; // tp、dp、pp...
};

// Summary性能数据
const std::string KEY_PREPARING_TIME = "preparing";
const std::string KEY_TOTAL_COMPUTING_TIME = "computing";
const std::string KEY_PURE_COMPUTING_TIME = "pureComputing";
const std::string KEY_TOTAL_COMMUNICATION = "communication";
const std::string KEY_COMMUNICATION_OVERLAPPED = "notOverlapped";
const std::string KEY_COMMUNICATION_NOT_OVERLAPPED = "overlapped";
const std::string KEY_COMMUNICATION_NOT_OVERLAPPED_AND_RECEIVE = "excludeReceive";
const std::string KEY_FREE_TIME = "free";
const std::string KEY_STAGE_TIME = "stage";
const std::string KEY_BUBBLE_TIME = "bubble";
const std::string KEY_NPU_TIME = "npuTime";
const std::string KEY_COMPUTING_RATIO = "computingRatio";
const std::string KEY_COMMUNICATION_RATIO = "communicationRatio";
const std::string KEY_MAX_SUFFIX = "Max";
const std::string KEY_MIN_SUFFIX = "Min";
const std::string KEY_RANGE_SUFFIX = "Range";

const std::string VALUE_COMPUTING_RATIO = "Computing Ratio";
const std::string VALUE_COMMUNICATION_RATIO = "Communication Ratio";
const std::string VALUE_MAX = "Max ";
const std::string VALUE_MIN = "Min ";
const std::string VALUE_RANGE = " Range";
const std::string VALUE_SUM_OF_MAX = "Sum of Max ";

// chart type
const std::string BAR_CHART = "bar";
const std::string LINE_CHART = "line";
// stack type
const std::string TIME_STACK = "time";
// y axis type
const std::string TIME_AXIS = "time";
const std::string RATIO_AXIS = "ratio";

// text格式数据，从profiler_metadata.json获取distributed_args并行策略信息
// db格式数据，当初始状态的数据库有ClusterBaseInfo表时，存储distributed_args列的并行策略信息
struct DistributedArgs {
    ParallelStrategyConfig config{MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 1, 1, 1, 1};
    uint32_t worldSize = 1;
    bool sequenceParallel = false;
};
const std::vector<std::string> DISTRIBUTED_ARGS_INT_KEY{"tensor_model_parallel_size", "pipeline_model_parallel_size",
    "data_parallel_size", "context_parallel_size", "expert_model_parallel_size", "world_size"};
const std::vector<std::string> DISTRIBUTED_ARGS_BOOL_KEY{"sequence_parallel"};

struct ExpertHotspotStruct {
    // 模型阶段：prefill和decode两种
    std::string modelStage;
    // 卡号，从0开始计数
    int rankId = 0;
    // 访问数
    uint64_t visits = 0;
    // 层级，从0开始计数
    int layer = 0;
    // 本地专家号，专家号在本地的顺序
    int localExpertId = 0;
    // 版本号（重置前、重置后）
    std::string version;
    // 全局专家号
    int expertId = 0;
    // 索引
    int expertIndex = 0;
};

struct ExpertDeploymentStruct {
    int deviceId = 0;
    std::vector<int> expertList;
    int layer = 0;
    std::string modelStage;
    std::string version;
};

struct ModelInfo {
    // 稠密层列表，数据格式0,1,2
    std::vector<int> denseLayerList;
    // moe层数
    int moeLayer = 0;
    // rank数量
    int rankNumber = 0;
    // 专家数量（专家序号的最大值）
    int expertNumber = 0;
    // 模型总层数
    int modelLayer = 0;
};

struct ModelGenConfig {
    // 模型moe层数
    uint64_t moeLayer = 0;
    // 数据保存间隔
    uint64_t collectionInterval = 0;
    // 模型的专家数
    uint64_t expertsNumber = 0;
    // 一个token挑选的专家数
    uint64_t expertsSelectedNumber = 0;
    // 外置共享专家数
    uint64_t expertsDanglingSharedNumber = 0;
};

const std::string KEY_DENSE_LAYER_LIST = "denseLayerList";
const std::string KEY_MOE_LAYER = "moeLayer";
const std::string KEY_RANK_NUMBER = "rankNumber";
const std::string KEY_EXPERT_NUMBER = "expertNumber";
const std::string KEY_MODEL_LAYER = "modelLayer";
const int CACHE_SIZE = 1024;
const std::string EXPERT_HOTSPOT_FILE_REG = R"((prefill|decode)_([0-9]{1,4})\.csv$)";
const std::string EXPERT_DEPLOYMENT_FILE_REG = R"((prefill|decode)_global_deployment\.json$)";
const std::string EXPERT_MODEL_GEN_CONFIG_REG = R"(model_gen_config\.json$)";

const std::string PACKET_ANALYZER_TITLE = "Packet Analysis";
struct PacketAnalyzerData {
    // 小包分析采集三项数据，第一项是链路方式，为SDMA或者RDMA之一
    std::string type;
    // 第二项是传输大小
    double transitSize;
    // 第三项是传输时长
    double transitTime;
};

const std::string BYTEALIGNMENT_ANALYZER_TITLE = "Byte Alignment Analysis";
struct CommunicationSmallOperatorInfo {
    uint64_t size = 0;
    std::string transportType;
    std::string linkType;
};
struct CommunicationLargeOperatorInfo {
    std::string name;
    std::vector<CommunicationSmallOperatorInfo> memcpyTasks;
    std::vector<CommunicationSmallOperatorInfo> reduceInlineTasks;
};
struct ByteAlignmentAnalyzerLargeOperatorInfo {
    std::string name;
};
struct ByteAlignmentAnalyzerSmallOperatorInfo {
    std::string name;
    std::string taskType;
    uint64_t size = 0;
    std::string transportType;
    std::string linkType;
};

const std::string BANDWIDTHCONTENTION_ANALYZER_TITLE = "Bandwidth Contention Analysis";
struct BandwidthContentionMatMulInfo {
    std::string name;
    double startTime = 0.0;
    double duration = 0.0;
};
struct BandwidthContentionSDMAInfo {
    std::string name;
    double startTime = 0.0;
    double duration = 0.0;
    double bandwidth = 0.0;
};
struct BandwidthContentionData {
    std::map<std::string, std::vector<BandwidthContentionMatMulInfo>> matMulData;
    std::map<std::string, std::vector<BandwidthContentionSDMAInfo>> SDMAData;
};

const std::string RETRANSMISSION_ANALYZER_TITLE = "Communication Retransmission Analysis";
struct RetransmissionClassificationInfo {
    std::string iterationId;
    std::string groupId;
    std::string opName;
    double minElapseTime;
    double maxRDMATransitTime;
};
} // end of namespace Module
} // end of namespace Dic
#endif // PROFILER_SERVER_CLUSTER_DEF_H
