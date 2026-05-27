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

#ifndef PROFILER_SERVER_SERVER_SRC_MODULES_SUMMARY_CORE_BASEPARALLELSTRATEGYALGORITHM_H
#define PROFILER_SERVER_SERVER_SRC_MODULES_SUMMARY_CORE_BASEPARALLELSTRATEGYALGORITHM_H

#include <unordered_map>
#include <functional>
#include "ClusterDef.h"
#include "SummaryProtocolResponse.h"
#include "SummaryProtocolRequest.h"
#include "ParallelStrategyAlgorithmDef.h"
#include "TopNAdviceMaintainer.h"
#include "SummaryErrorManager.h"

namespace Dic::Module::Summary {

struct ParallelDetails {
    bool isShown = false; // shownMap 代表当前层次是否展示
    uint32_t size = 1; // sizeMap ppSize等。若当前层次已折叠，size置1
};

const std::vector<std::string> LAYOUT = {DP_PARA, PP_PARA, CP_PARA, TP_PARA};

class BaseParallelStrategyAlgorithm {
  public:
    BaseParallelStrategyAlgorithm() = default;
    virtual ~BaseParallelStrategyAlgorithm() = default;

    virtual void SetStrategyConfig(const ParallelStrategyConfig &config);
    ParallelStrategyConfig GetStrategyConfig();
    void ClearStrategyConfigCache();
    virtual bool UpdateParallelDimension(
        const std::string &dimension, const ParallelStrategyConfig &tmpConfig, std::string &err) = 0;
    virtual bool GenerateArrangementByDimension(std::string &err) = 0;
    ArrangementAndConnectionData GetArrangementData();
    virtual bool GetPerformanceIndicatorByDimension(const GetPerformanceIndicatorParam &params,
        const std::unordered_map<std::uint32_t, StepStatistic> &statistic,
        std::vector<IndicatorDataStruct> &indicatorData, std::string &err) = 0;
    virtual void CalAdviceInfo(const std::string &dimension, std::vector<std::string> &advices,
        std::vector<IndicatorDataStruct> &indicatorData);
    virtual std::vector<Connection> GetAllCommunicationGroups(std::string &err) = 0;
    virtual CommInfoMap GetCommInfoByDimension(const CommInfoMap &expandCommInfos, const std::string &curDimension);
    // calculate slow rank info by commInfo under rank
    bool CalAdviceInfoByCommInfo(CommInfoMap &commInTpDimension);
    std::vector<AdviceInfoForSlowRank> GetTopNAdviceInfo(bool &matchSuccess);

  protected:
    uint32_t GetParallelSizeByType(const std::string &type) const;
    void CalStrategyConfig(const std::string &tmpDimension, const ParallelStrategyConfig &tmpConfig);

    // get arrangements
    void ClearArrangementData();
    bool UpdateShowMap(std::string &err);
    void SetParaDetail(const std::string &para, uint32_t size);
    void UpdateElementSize();
    std::string GetElementName(std::unordered_map<std::string, uint32_t> &indexAttributes);
    Position GetElementPosition(std::unordered_map<std::string, uint32_t> &indexAttributes) const;
    uint32_t CalculateContainingRanksByAttrs(
        uint32_t dpIndex, uint32_t ppIndex, uint32_t cpIndex, uint32_t tpIndex) const;
    static std::string FormatRanksForInterval(uint32_t start, uint32_t end);
    static std::string FormatRanksForSeveralIntervals(const std::vector<std::string> &intervals);
    std::vector<uint32_t> GetElementContainRanks(
        uint32_t index, std::unordered_map<std::string, uint32_t> &attrs, std::string &formattedRanks);
    std::vector<uint32_t> GetElementContainFormattedRanks(std::unordered_map<std::string, uint32_t> &attrs,
        std::string &formattedRanks, const ElementRankDetails &details);

    // get performance indicator
    void SetTpIndicatorAttr();
    void SetPpIndicatorAttr();
    void SetCpIndicatorAttr();
    void SetDpIndicatorAttr();

    // get performance data
    virtual void CalculatePerformanceDataWithTpDimension(
        const std::unordered_map<std::uint32_t, StepStatistic> &statistic,
        std::vector<IndicatorDataStruct> &indicatorData);
    void ReduceTpPerformance(const std::unordered_map<std::uint32_t, StepStatistic> &statistic);
    virtual void CalculatePerformanceDataWithCpDimension(std::vector<IndicatorDataStruct> &indicatorData);
    void ReduceCpPerformance();
    void CalculatePerformanceDataWithPpDimension(std::vector<IndicatorDataStruct> &indicatorData);
    void ReducePpPerformanceForDpLast();
    void ReducePpPerformanceForPpLast();
    void ReducePpPerformance(uint32_t startIndex, uint32_t step, uint32_t &dpGroupIdx);
    static void GetPerformanceResponseDataWithDpDimension(
        const std::unordered_map<std::uint32_t, StepStatistic> &statistic,
        std::vector<IndicatorDataStruct> &indicatorData);
    static double Reserved3DecimalPlaces(double num);
    static void AnalyzePerformanceAdviceWithDpCpPpTpDimension(Protocol::TraceStatistic &max,
        Protocol::TraceStatistic &min, double meanE2ETime, std::vector<std::string> &advices);

    std::unordered_map<std::string, std::vector<CommInfoUnderRank>> ReduceCommDefaultFunc(
        const std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &input, uint32_t w, uint32_t h);

    // calculate slow rank info by commInfo under rank
    TopNAdviceMaintainer CalAdviceInfoByPpDim(const CommInfoMap &commInTpDimension);
    uint32_t GetElementIndex(
        std::unordered_map<std::string, uint32_t> &indexAttributes, const ParallelStrategyConfig &tmpConfig) const;
    static std::string GetElementNameForTopNAdvice(
        const ParallelStrategyConfig &tmpConfig, std::unordered_map<std::string, uint32_t> &indexAttributes);
    static uint32_t GetTempParallelSizeByTypeForTopNAdvice(
        const std::string &type, const ParallelStrategyConfig &config);
    TopNAdviceMaintainer CalAdviceInfoByCpDim(
        const TopNAdviceMaintainer &topNAdviceForPpDim, const CommInfoMap &commInTpDimension);
    TopNAdviceMaintainer CalAdviceInfoByTpDim(
        const TopNAdviceMaintainer &topNAdviceForCpDim, CommInfoMap &commInTpDimension);
    void CalTpDimAdviceInfoWithoutDpCpAdvice(const ParallelStrategyConfig &tmpConfig, CommInfoMap &commInTpDimension,
        TopNAdviceMaintainer &topNAdviceForTpDim);

    void CalSynchronizeTime(const std::string &para, AdviceInfoForSlowRank &adviceInfo,
        const ParallelStrategyConfig &tmpConfig, CommInfoMap &commInDimension, TopNAdviceMaintainer &topNAdvice);

    ParallelStrategyConfig strategyConfig;
    bool orderIsTpPpDp = false; // 用于区分算法排布顺序是TP-PP-DP类型还是TP-DP-PP类型, 默认为TP-DP-PP类型
    std::string dimension = DIMENSIONS_TP; // 默认全展开
    uint32_t wordSize = 1;
    uint32_t tpSize = 1;
    uint32_t tpCpSize = 1;
    uint32_t tpCpDpSize = 1;
    uint32_t tpCpPpSize = 1;

    // 折叠视图
    uint32_t elementSize = 1;
    uint32_t foldedTpSize = 1;
    uint32_t foldedTpCpSize = 1;
    uint32_t foldedTpCpDpSize = 1;
    uint32_t foldedTpCpPpSize = 1;

    ArrangementAndConnectionData data;

    // 并行顺序（包含size=1的域）
    std::vector<std::string> paraOrder{};
    // 并行顺序（包含size=1的域）且考虑ep域
    std::vector<std::string> paraOrderWithEp{};
    // 并行顺序（不包含size=1的域）, 用于计算连线
    std::vector<std::string> updatedOrder{};
    // 并行顺序（不包含size=1的域）且考虑ep域, 用于计算连线
    std::vector<std::string> updatedOrderWithEp{};
    // 按照updatedOrder顺序的并行策略尺寸
    std::vector<uint32_t> parallelSize{};
    // 按照updatedOrderWithEp顺序的并行策略尺寸, 会对原有DP Size进行拆分,仅影响连线生成，不影响布局
    std::vector<uint32_t> parallelSizeWithEp{};
    std::vector<Connection> allCommunicationGroups{}; // 全量通信域
    std::unordered_map<std::string, ParallelDetails> paraDetailsMap; // 记录某并行域是否被折叠

    // get performance data
    std::unordered_map<std::uint32_t, StepStatistic> reduceTpMax;
    std::unordered_map<std::uint32_t, StepStatistic> reduceTpMin;
    std::unordered_map<std::uint32_t, StepStatistic> reduceCpMax;
    std::unordered_map<std::uint32_t, StepStatistic> reduceCpMin;
    std::unordered_map<std::uint32_t, StepStatistic> reducePpStatistic;
    const static inline int cpSizeWithEp = 1;
    const static inline int reservedNum = 3; // 保留3位小数
    const static inline int epPosPpLast = 2; // tp-cp-ep-dp-pp
    const static inline uint32_t maxLengthOfAdvice = 10; // 专家建议优先队列最大容量
    const static inline uint32_t topN = 3; // 专家建议取TopN慢卡
    const static inline double thresholdForSlowRankAdvice = 0.05; // 慢卡/慢分组 通信同步时间阈值

    // slow rank advice by CommInfo under rank
    std::vector<AdviceInfoForSlowRank> slowRankAdvice;
    bool commMatchSuccess;

    using CommInfoHandler = std::function<std::unordered_map<std::string, std::vector<CommInfoUnderRank>>(
        const std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &)>;
    std::map<std::string, CommInfoHandler> commInfoHandlers;
    std::unordered_map<std::string, std::vector<CommInfoUnderRank>> ReduceCommTpDimensionDef(
        const std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &expendData);
    std::unordered_map<std::string, std::vector<CommInfoUnderRank>> ReduceCommCpDimensionDef(
        const std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &expendData);
    std::unordered_map<std::string, std::vector<CommInfoUnderRank>> ReduceCommPpDimensionDef(
        const std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &expendData);
};
}
#endif // PROFILER_SERVER_SERVER_SRC_MODULES_SUMMARY_CORE_BASEPARALLELSTRATEGYALGORITHM_H
