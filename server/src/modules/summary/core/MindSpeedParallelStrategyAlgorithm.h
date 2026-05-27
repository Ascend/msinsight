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

#ifndef PROFILER_SERVER_MINDSPEEDPARALLELSTRATEGYALGORITHM_H
#define PROFILER_SERVER_MINDSPEEDPARALLELSTRATEGYALGORITHM_H

#include "BaseParallelStrategyAlgorithm.h"
#include "ParallelStrategyAlgorithmHelper.h"
#include "ParallelStrategyAlgorithmDef.h"

namespace Dic::Module::Summary {
class MindSpeedParallelStrategyAlgorithm : public BaseParallelStrategyAlgorithm {
  public:
    static const std::unordered_map<std::string, std::string> tokenExceptEp;
    static const std::unordered_map<std::string, std::string> tokenWithEp;
    static const std::unordered_map<std::string, std::string> tokenOfTp2dNd1;
    static const std::unordered_map<std::string, std::string> tokenOfTp2dNd2;
    MindSpeedParallelStrategyAlgorithm();
    ~MindSpeedParallelStrategyAlgorithm() override;

    void SetStrategyConfig(const ParallelStrategyConfig &config) override;
    bool UpdateParallelDimension(
        const std::string &tmpDimension, const ParallelStrategyConfig &tmpConfig, std::string &err) override;
    bool GenerateArrangementByDimension(std::string &err) override;
    bool GetPerformanceIndicatorByDimension(const GetPerformanceIndicatorParam &performanceParams,
        const std::unordered_map<std::uint32_t, StepStatistic> &statistic,
        std::vector<IndicatorDataStruct> &indicatorData, std::string &err) override;
    void CalAdviceInfo(const std::string &dimension, std::vector<std::string> &advices,
        std::vector<IndicatorDataStruct> &indicatorData) override;
    // get all communication groups
    std::vector<Connection> GetAllCommunicationGroups(std::string &err) override;

  private:
    // get arrangements
    void SetIndicatorAttr();
    void GetPerArrangement(uint32_t index, std::unordered_map<std::string, uint32_t> &indexAttributes);
    void UpdateIndexAttributes(std::unordered_map<std::string, uint32_t> &indexAttributes);
    // get connections
    void UpdateOrderAndParallelSize();
    bool GetConnectionsByTokenList(std::string &err);
    bool GetConnectionsByMegatronToken(std::string &err, bool independentEp);
    bool GetConnectionsForCpUlyssesAndRing(
        std::vector<std::string> &updatedOrderForCp, std::vector<uint32_t> &updatedParallelSizeForCp, std::string &err);
    bool GetConnectionsForCpDoubleRing(
        std::vector<std::string> &updatedOrderForCp, std::vector<uint32_t> &updatedParallelSizeForCp, std::string &err);
    bool GetConnectionsForTp2d(std::string &err, const std::vector<std::string> &tokenList,
        const std::vector<uint32_t> &sizeList, const std::unordered_map<std::string, std::string> &tokenMap);
    static bool ReplaceParallelGroup(const std::string &para, std::vector<std::string> &order,
        std::vector<uint32_t> &paraSize, const std::vector<std::string> &orderList,
        const std::vector<uint32_t> &sizeList);

    const static inline int epPosPpLast = 2; // tp-cp-ep-dp-pp

    // parameter for cp
    bool useHybridCp = false; // use MINDSPEED_HYBIRD_CP_ALG or MINDSPEED_HYBIRD_ADAPTIVE_CP_ALG
    uint32_t ulyssesDegree = 1;
    uint32_t ringDegree = 1;
    uint32_t winSize = 1;
    uint32_t interSize = 1;
    uint32_t nd1dim1 = 1;
    uint32_t nd1dim2 = 1;
    uint32_t nd2dim1 = 1;
    uint32_t nd2dim2 = 1;
};
}

#endif // PROFILER_SERVER_MINDSPEEDPARALLELSTRATEGYALGORITHM_H
