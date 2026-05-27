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

#ifndef PROFILER_SERVER_MINDIELLMPARALLELSTRATEGYALGORITHM_H
#define PROFILER_SERVER_MINDIELLMPARALLELSTRATEGYALGORITHM_H

#include "BaseParallelStrategyAlgorithm.h"
#include "ParallelStrategyAlgorithmHelper.h"
#include "ParallelStrategyAlgorithmDef.h"

namespace Dic::Module::Summary {
class MindIELLMParallelStrategyAlgorithm : public BaseParallelStrategyAlgorithm {
  public:
    static const std::unordered_map<std::string, std::string> tokenExceptEp;
    static const std::unordered_map<std::string, std::string> tokenWithEp;
    MindIELLMParallelStrategyAlgorithm();
    ~MindIELLMParallelStrategyAlgorithm() override;
    bool UpdateParallelDimension(
        const std::string &tmpDimension, const ParallelStrategyConfig &tmpConfig, std::string &err) override;
    bool GenerateArrangementByDimension(std::string &err) override;
    bool GetPerformanceIndicatorByDimension(const GetPerformanceIndicatorParam &performanceParams,
        const std::unordered_map<std::uint32_t, StepStatistic> &statistic,
        std::vector<IndicatorDataStruct> &indicatorData, std::string &err) override;
    void CalAdviceInfo(const std::string &dimension, std::vector<std::string> &advices,
        std::vector<IndicatorDataStruct> &indicatorData) override;
    std::vector<Connection> GetAllCommunicationGroups(std::string &err) override;
    CommInfoMap GetCommInfoByDimension(const CommInfoMap &expandCommInfos, const std::string &dimension) override;

  private:
    enum class ParaMode { TP_DP_PP, MOE_TP_EP_PP };
    void UpdateOrderAndParallelSize();
    void SetIndicatorAttr();
    void GetPerArrangement(uint32_t index, std::unordered_map<std::string, uint32_t> &indexAttributes);
    void UpdateIndexAttributes(std::unordered_map<std::string, uint32_t> &indexAttributes);
    bool GetConnectionsByTokenList(std::string &err);
    bool GetConnectionsByToken(std::string &err, ParaMode mode);
};
}

#endif // PROFILER_SERVER_MINDIELLMPARALLELSTRATEGYALGORITHM_H
