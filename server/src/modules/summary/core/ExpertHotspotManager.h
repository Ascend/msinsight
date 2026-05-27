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

#ifndef PROFILER_SERVER_EXPERTHOTSPOTMANAGER_H
#define PROFILER_SERVER_EXPERTHOTSPOTMANAGER_H
#include <string>
#include "ClusterDef.h"
#include "VirtualClusterDatabase.h"
#include "DBConnectionPool.h"
#include "DataBaseManager.h"

namespace Dic {
namespace Module {
namespace Summary {
struct FillExpertDataParams {
    ModelInfo modelInfo;
    std::vector<ExpertHotspotStruct> hotspotInfos;
    std::vector<ExpertDeploymentStruct> deployment;
    std::vector<int> moeLayerMapping;
    int colNumber = 0;
    uint64_t expertNumberPerRank = 0;
    std::set<int> denseLayerSet;
};

struct ExtractHeatMapParams {
    std::string rankId;
    FullDb::DataType dataType;
    std::vector<std::string> cannApiList;
    std::vector<std::string> hardwareOperatorList;
    std::string clusterPath;
};

class ExpertHotspotManager {
  public:
    static bool InitExpertHotspotData(
        const std::string &filePath, const std::string &version, std::string &errorMsg, const std::string &clusterPath);
    static std::vector<ExpertHotspotStruct> QueryExpertHotspotData(
        const std::string &clusterPath, const std::string &modelStage, const std::string &version);
    static ModelInfo GetModelInfo(const std::string &clusterPath);
    static bool UpdateModelInfo(const std::string &clusterPath, ModelInfo &newModelInfo, std::string &errorMsg);
    static bool UpdateHeatMapFromProfiling(
        std::string &errorMsg, const std::string &clusterPath, const std::vector<std::string> &rankIdList);

  private:
    // CANN层，每一个layer执行API
    const static inline std::vector<std::string> layerExecuteApiNameList = {"Prefill_layer::Execute",
        "AscendCL@Decoder_layer::Execute", "AscendCL@Prefill_layer::Execute", "Decoder_layer::Execute"};
    // CANN层，每一个groupedMatmul执行API
    const static inline std::vector<std::string> groupedMatmulApiNameList = {
        "AscendCL@aclnnGroupedMatmulV4", "AscendCL@aclnnGroupedMatmulSwiglu", "aclnnGroupedMatmulV4"};
    // CANN层，每一个LmHead执行API，代表一个模型的结束
    const static inline std::vector<std::string> lmHeadApiNameList = {"AscendCL@LmHead::Execute", "LmHead::Execute"};
    // Hardware层，每一个groupedMatmul算子名
    const static inline std::vector<std::string> groupedMatmulComputeNameList = {
        "aclnnGroupedMatmulV4_GroupedMatmul_GroupedMatmul",
        "aclnnGroupedMatmulSwiglu_GroupedMatmulSwiglu_GroupedMatmulSwiglu"};
    static bool FillExpertInfo(std::vector<ExpertHotspotStruct> &hotspotInfos, const ModelInfo &modelInfo,
        const std::vector<ExpertDeploymentStruct> &deployment);
    static std::map<std::string, ModelInfo> ParseHotspotData(const std::vector<std::string> &hotspotFiles,
        std::string &errorMsg, std::shared_ptr<VirtualClusterDatabase> &database, const std::string &version,
        const ModelGenConfig &config);
    static std::map<std::string, ModelInfo> ParseDeploymentData(const std::vector<std::string> &deploymentFiles,
        std::string &errorMsg, std::shared_ptr<VirtualClusterDatabase> &database, const std::string version);
    static bool MergeAndSaveModelInfo(const std::map<std::string, ModelInfo> &hotspotModelInfo,
        const std::map<std::string, ModelInfo> &deploymentModelInfo, std::shared_ptr<VirtualClusterDatabase> &database);
    static bool SaveModelInfo(const ModelInfo &modelInfo, std::shared_ptr<VirtualClusterDatabase> &db);
    static ModelInfo GetModelInfo(std::shared_ptr<VirtualClusterDatabase> &db);
    static std::vector<int> CalMoeLayerMapping(const ModelInfo &modelInfo, const std::set<int> &denseLayerSet);
    static bool FillHotspotData(std::vector<ExpertHotspotStruct> &res, FillExpertDataParams &params);
    static bool FillDeploymentData(std::vector<ExpertHotspotStruct> &res, FillExpertDataParams &params);
    static void FillDenseLayerInfo(std::vector<ExpertHotspotStruct> &res, FillExpertDataParams &params);
    static bool ExtractHeatMapFromTraceDb(
        const ExtractHeatMapParams &params, ModelInfo &modelInfo, std::string &errorMsg);
    static std::map<std::string, ExpertHotspotStruct> CalHeatMap(const int &rankId,
        const std::vector<FullDb::CompeteSliceDomain> &cannApiSliceList,
        const std::vector<FullDb::CompeteSliceDomain> &hardwareSliceList, ModelInfo &modelInfo);
};
}
}
}
#endif // PROFILER_SERVER_EXPERTHOTSPOTMANAGER_H
