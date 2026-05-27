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
#include <algorithm>
#include "ExpertHotspotParser.h"
#include "ExpertDeploymentParser.h"
#include "DataBaseManager.h"
#include "ClusterDef.h"
#include "CollectionUtil.h"
#include "NumberSafeUtil.h"
#include "NumberUtil.h"
#include "RenderEngine.h"
#include "CollectionUtil.h"
#include "ModelGenConfigParser.h"
#include "ExpertHotspotManager.h"
#include "SummaryErrorManager.h"

namespace Dic {
namespace Module {
namespace Summary {

std::map<std::string, ModelInfo> ExpertHotspotManager::ParseHotspotData(const std::vector<std::string> &hotspotFiles,
    std::string &errorMsg, std::shared_ptr<VirtualClusterDatabase> &database, const std::string &version,
    const ModelGenConfig &config) {
    // 清空老数据
    if (!database->DeleteExpertHotspot("", version)) {
        errorMsg = "Failed to clear old expert hotspot data, version:" + version;
        SetSummaryError(ErrorCode::CLEAR_EXPERT_HOTSPOT_FAILED);
        return {};
    }
    ExpertHotspotParser parser(database, config);
    for (const auto &item : hotspotFiles) {
        // 文件解析，单个文件解析失败不影响最终结果
        if (!parser.Parse(item, version)) {
            ServerLog::Warn("Fail to parser file:", item);
        }
    }
    database->SaveExpertHotspot();
    return parser.GetModelInfoMap();
}

std::map<std::string, ModelInfo> ExpertHotspotManager::ParseDeploymentData(
    const std::vector<std::string> &deploymentFiles, std::string &errorMsg,
    std::shared_ptr<VirtualClusterDatabase> &database, const std::string version) {
    // 清空老数据
    if (!database->DeleteDeployment("", version)) {
        errorMsg = "Failed to clear old expert deployment data, version:" + version;
        SetSummaryError(ErrorCode::CLEAR_DEPLOYMENT_FAILED);
        return {};
    }
    // 文件解析
    ExpertDeploymentParser parser(database);
    for (const auto &item : deploymentFiles) {
        if (!parser.Parse(item, version)) {
            ServerLog::Warn("Fail to parser file:", item);
        }
    }
    database->SaveExpertDeployment();
    return parser.GetModelInfoMap();
}

bool ExpertHotspotManager::MergeAndSaveModelInfo(const std::map<std::string, ModelInfo> &hotspotModelInfo,
    const std::map<std::string, ModelInfo> &deploymentModelInfo, std::shared_ptr<VirtualClusterDatabase> &database) {
    // 获取当前db中已保存的modelInfo信息,稠密层列表从老数据中直接保留，模型总层数取当前值和新导入数据的最大值
    ModelInfo curModelInfo = GetModelInfo(database);
    ModelInfo hotspot = hotspotModelInfo.empty() ? ModelInfo() : hotspotModelInfo.begin()->second;
    ModelInfo deployment = deploymentModelInfo.empty() ? ModelInfo() : deploymentModelInfo.begin()->second;
    bool isAllExist = !hotspotModelInfo.empty() && !deploymentModelInfo.empty();
    bool isParamsNotEqual = hotspot.moeLayer != deployment.moeLayer || hotspot.rankNumber != deployment.rankNumber;
    if (isAllExist && isParamsNotEqual) {
        return false;
    }
    ModelInfo finalModelInfo;
    finalModelInfo.denseLayerList = curModelInfo.denseLayerList;
    finalModelInfo.modelLayer = curModelInfo.modelLayer;
    // 配置信息保存
    if (!hotspotModelInfo.empty()) {
        // 计算总层数，从以下两个数据中取大的数据：1.导入数据的moe层数+已配置的稠密层数；2.已配置的总层数
        uint64_t totalModelLayerCeil = NumberSafe::Add(hotspot.moeLayer, curModelInfo.denseLayerList.size());
        int totalModelLayer =
            static_cast<int>(NumberUtil::CeilingClamp(totalModelLayerCeil, static_cast<uint64_t>(INT_MAX)));
        finalModelInfo.modelLayer = std::max(totalModelLayer, finalModelInfo.modelLayer);
        finalModelInfo.rankNumber = hotspot.rankNumber;
        finalModelInfo.moeLayer = hotspot.moeLayer;
        finalModelInfo.expertNumber = hotspot.expertNumber;
    }

    if (!deploymentModelInfo.empty()) {
        uint64_t totalModelLayerCeil = NumberSafe::Add(deployment.moeLayer, curModelInfo.denseLayerList.size());
        int totalModelLayer =
            static_cast<int>(NumberUtil::CeilingClamp(totalModelLayerCeil, static_cast<uint64_t>(INT_MAX)));
        finalModelInfo.modelLayer = std::max(totalModelLayer, finalModelInfo.modelLayer);
        finalModelInfo.rankNumber = deployment.rankNumber;
        finalModelInfo.moeLayer = deployment.moeLayer;
        finalModelInfo.expertNumber = deployment.expertNumber;
    }

    return SaveModelInfo(finalModelInfo, database);
}

bool ExpertHotspotManager::InitExpertHotspotData(
    const std::string &filePath, const std::string &version, std::string &errorMsg, const std::string &clusterPath) {
    // 参数校验，ConvertToRealPath方法中会调用CheckDirValid方法对文件进行校验
    std::string realFilePath = filePath;
    if (!FileUtil::ConvertToRealPath(errorMsg, realFilePath)) {
        SetSummaryError(ErrorCode::GET_REAL_PATH_FAILED);
        return false;
    }
    // 获取db
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    if (database == nullptr) {
        errorMsg = "Cluster database is not exist.";
        SetSummaryError(ErrorCode::CONNECT_DATABASE_FAILED);
        return false;
    }

    // 查找文件列表
    auto modelConfigFiles = FileUtil::FindAllFilesByRegex(realFilePath, std::regex(EXPERT_MODEL_GEN_CONFIG_REG));
    auto hotspotFiles = FileUtil::FindAllFilesByRegex(realFilePath, std::regex(EXPERT_HOTSPOT_FILE_REG));
    auto deploymentFiles = FileUtil::FindAllFilesByRegex(realFilePath, std::regex(EXPERT_DEPLOYMENT_FILE_REG));
    bool isParseHotspot = !modelConfigFiles.empty() && !hotspotFiles.empty();
    bool isParseDeployment = !deploymentFiles.empty();
    if (!isParseHotspot && !isParseDeployment) {
        errorMsg = "No parsable files found";
        SetSummaryError(ErrorCode::GET_PARSED_FILES_FAILED);
        return false;
    }

    // 文件内容解析并落库
    std::map<std::string, ModelInfo> hotspotModelInfo;
    if (isParseHotspot) {
        ModelGenConfig config = ModelGenConfigParser::ParserModelGenConfigByFilePath(modelConfigFiles[0], errorMsg);
        hotspotModelInfo = ParseHotspotData(hotspotFiles, errorMsg, database, version, config);
    }
    std::map<std::string, ModelInfo> deploymentModelInfo;
    if (isParseDeployment) {
        deploymentModelInfo = ParseDeploymentData(deploymentFiles, errorMsg, database, version);
    }
    if (!errorMsg.empty()) {
        return false;
    }
    // 配置信息保存
    bool res = MergeAndSaveModelInfo(hotspotModelInfo, deploymentModelInfo, database);
    if (!res) {
        SetSummaryError(ErrorCode::GET_PARSED_FILES_FAILED);
        return false;
    }
    return true;
}

bool ExpertHotspotManager::SaveModelInfo(const ModelInfo &modelInfo, std::shared_ptr<VirtualClusterDatabase> &db) {
    std::map<std::string, std::string> modelInfoMap;
    modelInfoMap[KEY_DENSE_LAYER_LIST] = StringUtil::join(modelInfo.denseLayerList, ",");
    // 只有数据不为0才展示
    if (modelInfo.moeLayer != 0) {
        modelInfoMap[KEY_MOE_LAYER] = std::to_string(modelInfo.moeLayer);
    }
    if (modelInfo.rankNumber != 0) {
        modelInfoMap[KEY_RANK_NUMBER] = std::to_string(modelInfo.rankNumber);
    }
    if (modelInfo.expertNumber != 0) {
        modelInfoMap[KEY_EXPERT_NUMBER] = std::to_string(modelInfo.expertNumber);
    }
    if (modelInfo.modelLayer != 0) {
        modelInfoMap[KEY_MODEL_LAYER] = std::to_string(modelInfo.modelLayer);
    }
    return db->InsertDuplicateUpdateBaseInfo(modelInfoMap);
}

bool ExpertHotspotManager::UpdateModelInfo(
    const std::string &clusterPath, ModelInfo &newModelInfo, std::string &errorMsg) {
    // 获取db
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    if (database == nullptr) {
        errorMsg = "Fail to update model info, database not exist.";
        SetSummaryError(ErrorCode::CONNECT_DATABASE_FAILED);
        return false;
    }
    ModelInfo curModelInfo = GetModelInfo(database);
    // 在已导入数据的场景下，专家数量不允许修改(这里通过rank数是否为0来判断是否导入过数据)
    if (curModelInfo.rankNumber != 0 && curModelInfo.expertNumber != 0 &&
        curModelInfo.expertNumber != newModelInfo.expertNumber) {
        errorMsg = "Fail to update model info, the number of expert number can't be modify.";
        SetSummaryError(ErrorCode::UPDATE_MODEL_INFO_MODIFY_FAILED);
        return false;
    }
    uint64_t totalLayer = NumberSafe::Add(curModelInfo.moeLayer, newModelInfo.denseLayerList.size());
    if (newModelInfo.modelLayer < 0 || static_cast<uint64_t>(newModelInfo.modelLayer) < totalLayer) {
        errorMsg = "Fail to update model info, "
                   "the sum of moe and dense layers is less than the total number of layers in the model.";
        SetSummaryError(ErrorCode::UPDATE_MODEL_INFO_NOT_EQUAL_FAILED);
        return false;
    }
    curModelInfo.modelLayer = newModelInfo.modelLayer;
    curModelInfo.expertNumber = newModelInfo.expertNumber;
    curModelInfo.denseLayerList = newModelInfo.denseLayerList;
    return SaveModelInfo(curModelInfo, database);
}

ModelInfo ExpertHotspotManager::GetModelInfo(const std::string &clusterPath) {
    // 获取db
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    if (database == nullptr) {
        ServerLog::Error("Fail to get model info, database not exist.");
        return {};
    }
    return GetModelInfo(database);
}

ModelInfo ExpertHotspotManager::GetModelInfo(std::shared_ptr<VirtualClusterDatabase> &db) {
    std::vector<std::string> keys = {
        KEY_DENSE_LAYER_LIST, KEY_MOE_LAYER, KEY_RANK_NUMBER, KEY_EXPERT_NUMBER, KEY_MODEL_LAYER};
    std::map<std::string, std::string> modelInfoMap = db->QueryBaseInfoByKeys(keys);
    std::string defaultZeroStr = "0";
    ModelInfo modelInfo;
    std::string denseLayerListStr =
        CollectionUtil::FindValueByKey(modelInfoMap, KEY_DENSE_LAYER_LIST, CollectionUtil::EMPTY_STRING);
    if (!denseLayerListStr.empty()) {
        for (const auto &item : StringUtil::Split(denseLayerListStr, ",")) {
            modelInfo.denseLayerList.push_back(StringUtil::StringToInt(item));
        }
    }
    modelInfo.expertNumber =
        StringUtil::StringToInt(CollectionUtil::FindValueByKey(modelInfoMap, KEY_EXPERT_NUMBER, defaultZeroStr));
    modelInfo.rankNumber =
        StringUtil::StringToInt(CollectionUtil::FindValueByKey(modelInfoMap, KEY_RANK_NUMBER, defaultZeroStr));
    modelInfo.moeLayer =
        StringUtil::StringToInt(CollectionUtil::FindValueByKey(modelInfoMap, KEY_MOE_LAYER, defaultZeroStr));
    modelInfo.modelLayer =
        StringUtil::StringToInt(CollectionUtil::FindValueByKey(modelInfoMap, KEY_MODEL_LAYER, defaultZeroStr));
    return modelInfo;
}

std::vector<int> ExpertHotspotManager::CalMoeLayerMapping(
    const ModelInfo &modelInfo, const std::set<int> &denseLayerSet) {
    // 计算从moe层映射到整体层的映射
    std::vector<int> moeLayerMapping(modelInfo.moeLayer);
    int moeLayerIndex = 0;
    for (int i = 0; i < modelInfo.modelLayer; ++i) {
        if (moeLayerIndex >= modelInfo.moeLayer) {
            break;
        }
        if (denseLayerSet.find(i) != denseLayerSet.end()) {
            continue;
        }
        moeLayerMapping[moeLayerIndex++] = i;
    }
    return moeLayerMapping;
}

bool ExpertHotspotManager::FillHotspotData(std::vector<ExpertHotspotStruct> &res, FillExpertDataParams &params) {
    for (auto &item : params.hotspotInfos) {
        if (item.layer >= params.modelInfo.moeLayer || item.rankId >= params.modelInfo.rankNumber) {
            ServerLog::Error("Invalid hotspot data.");
            return false;
        }
        // 更新layer为全局layer（包含稠密层的情况）
        item.layer = params.moeLayerMapping[item.layer];
        uint64_t expertId =
            NumberSafe::Add(NumberSafe::Muls(item.rankId, params.expertNumberPerRank), item.localExpertId);
        uint64_t expertIndex =
            NumberSafe::Add(NumberSafe::Muls(item.rankId, params.expertNumberPerRank), item.localExpertId);
        item.expertId = static_cast<int>(NumberUtil::CeilingClamp(expertId, static_cast<uint64_t>(INT_MAX)));
        item.expertIndex = static_cast<int>(NumberUtil::CeilingClamp(expertIndex, static_cast<uint64_t>(INT_MAX)));
        int index = NumberSafe::Add(item.expertIndex, NumberSafe::Muls(item.layer, params.colNumber));
        if (index >= NumberSafe::Muls(params.colNumber, params.modelInfo.modelLayer)) {
            return false;
        }
        res[index] = item;
    }
    return true;
}

bool ExpertHotspotManager::FillDeploymentData(std::vector<ExpertHotspotStruct> &res, FillExpertDataParams &params) {
    for (const auto &item : params.deployment) {
        if (item.layer >= params.modelInfo.moeLayer || item.deviceId >= params.modelInfo.rankNumber) {
            ServerLog::Error("Invalid deployment data.");
            return false;
        }
        int aclLayer = params.moeLayerMapping[item.layer];
        for (size_t i = 0; i < item.expertList.size(); ++i) {
            uint64_t expertIndex = NumberSafe::Add(i, NumberSafe::Muls(params.expertNumberPerRank, item.deviceId));
            uint64_t index = NumberSafe::Add(expertIndex, NumberSafe::Muls(aclLayer, params.colNumber));
            if (index >= static_cast<uint64_t>(NumberSafe::Muls(params.colNumber, params.modelInfo.modelLayer))) {
                return false;
            }
            res[index].expertIndex =
                static_cast<int>(NumberUtil::CeilingClamp(expertIndex, static_cast<uint64_t>(INT_MAX)));
            res[index].layer = aclLayer;
            res[index].expertId = item.expertList[i];
            res[index].rankId = item.deviceId;
        }
    }
    return true;
}

void ExpertHotspotManager::FillDenseLayerInfo(std::vector<ExpertHotspotStruct> &res, FillExpertDataParams &params) {
    for (int item = 0; item < params.modelInfo.modelLayer; ++item) {
        int startIndex = NumberSafe::Muls(item, params.colNumber);
        if (startIndex == 0 && item != 0 && params.colNumber != 0) {
            ServerLog::Error("The product of layerNum and colNum exceeds the limit of int");
            return;
        }
        for (int i = 0; i < params.colNumber; ++i) {
            int index = startIndex + i;
            res[index].expertIndex = i;
            res[index].expertId = -1;
            res[index].layer = item;
            if (params.expertNumberPerRank != 0) {
                // rankId计算（向下取整）：列数 ÷ 每个rank的专家数
                res[index].rankId = static_cast<int>(NumberUtil::CeilingClamp(
                    static_cast<uint64_t>(i) / params.expertNumberPerRank, static_cast<uint64_t>(INT_MAX)));
            }
        }
    }
}

bool ExpertHotspotManager::FillExpertInfo(std::vector<ExpertHotspotStruct> &hotspotInfos, const ModelInfo &modelInfo,
    const std::vector<ExpertDeploymentStruct> &deployment) {
    // 获取每个rank的专家数，以及热力图的列数目
    uint64_t expertNumberPerRank = 0;
    int colNumber = 0;
    if (!hotspotInfos.empty()) {
        std::vector<ExpertHotspotStruct> filteredHotspots;
        std::copy_if(hotspotInfos.begin(), hotspotInfos.end(), std::back_inserter(filteredHotspots),
            [hotspotInfos](const ExpertHotspotStruct &info) {
                return info.layer == hotspotInfos[0].layer && info.rankId == hotspotInfos[0].rankId;
            });
        expertNumberPerRank = filteredHotspots.size();
        colNumber = static_cast<int>(
            NumberUtil::CeilingClamp(expertNumberPerRank * modelInfo.rankNumber, static_cast<uint64_t>(INT_MAX)));
    } else if (!deployment.empty()) {
        // 如果有配置文件，则专家数直接取自数据内
        expertNumberPerRank = deployment[0].expertList.size();
        colNumber = static_cast<int>(
            NumberUtil::CeilingClamp(expertNumberPerRank * modelInfo.rankNumber, static_cast<uint64_t>(INT_MAX)));
    } else {
        colNumber = modelInfo.expertNumber;
    }

    // 初始化结果列表
    std::vector<ExpertHotspotStruct> res(colNumber * modelInfo.modelLayer);
    std::set<int> denseLayerSet(modelInfo.denseLayerList.begin(), modelInfo.denseLayerList.end());

    // 计算从moe层映射到整体层的映射
    std::vector<int> moeLayerMapping = CalMoeLayerMapping(modelInfo, denseLayerSet);

    FillExpertDataParams params{
        modelInfo, hotspotInfos, deployment, moeLayerMapping, colNumber, expertNumberPerRank, denseLayerSet};
    // 所有数据填充空内容
    FillDenseLayerInfo(res, params);

    // 热点数据填充，如果热点数据不存在，会直接返回true，不阻塞整体流程
    if (!FillHotspotData(res, params)) {
        return false;
    }

    if (!FillDeploymentData(res, params)) {
        return false;
    }

    hotspotInfos = res;
    return true;
}

std::vector<ExpertHotspotStruct> ExpertHotspotManager::QueryExpertHotspotData(
    const std::string &clusterPath, const std::string &modelStage, const std::string &version) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    if (database == nullptr) {
        return {};
    }
    auto hotspotRes = database->QueryExpertHotspotData(modelStage, version);
    auto deploymentRes = database->QueryExpertDeployment(modelStage, version);
    auto modelInfo = GetModelInfo(database);
    if (!FillExpertInfo(hotspotRes, modelInfo, deploymentRes)) {
        return {};
    }
    return hotspotRes;
}

bool ExpertHotspotManager::ExtractHeatMapFromTraceDb(
    const ExtractHeatMapParams &params, ModelInfo &modelInfo, std::string &errorMsg) {
    if (params.rankId.empty()) {
        errorMsg = "Fail to get extract heat map, file id is empty.";
        return false;
    }
    auto clusterDb = Timeline::DataBaseManager::Instance().GetClusterDatabase(params.clusterPath);
    if (clusterDb == nullptr) {
        errorMsg = "Fail to get extract heat map, database not exist.";
        return false;
    }
    // 获取cann层数据内容
    std::vector<FullDb::CompeteSliceDomain> cannApiSliceList =
        FullDb::RenderEngine::Instance()->QuerySliceDetailByNameList(
            params.rankId, params.dataType, "CANN", params.cannApiList);
    if (cannApiSliceList.empty()) {
        return false;
    }
    // 获取计算算子数据内容
    std::vector<FullDb::CompeteSliceDomain> hardwareSliceList =
        FullDb::RenderEngine::Instance()->QuerySliceDetailByNameList(
            params.rankId, params.dataType, "Ascend Hardware", params.hardwareOperatorList);
    if (hardwareSliceList.empty()) {
        return false;
    }
    int rankId = StringUtil::ExtractDigitRankIdFromHost(params.rankId);
    if (rankId < 0 || rankId == INT_MAX) {
        errorMsg = "Fail to get extract heat map, invalid rank id.";
        return false;
    }
    modelInfo.rankNumber = std::max(rankId + 1, modelInfo.rankNumber);
    auto heatMapData = CalHeatMap(rankId, cannApiSliceList, hardwareSliceList, modelInfo);
    for (const auto &item : heatMapData) {
        clusterDb->InsertExpertHotspotDataForCache(item.second);
    }
    clusterDb->SaveExpertHotspot();
    return true;
}

std::map<std::string, ExpertHotspotStruct> ExpertHotspotManager::CalHeatMap(const int &rankId,
    const std::vector<FullDb::CompeteSliceDomain> &cannApiSliceList,
    const std::vector<FullDb::CompeteSliceDomain> &hardwareSliceList, ModelInfo &modelInfo) {
    // 遍历数据并映射
    std::vector<int> denseLayerList;
    int layer = -1;
    size_t hardwareIndex = 0;
    size_t index = 0;
    std::string modelStage;
    std::map<std::string, ExpertHotspotStruct> res;
    while (index < cannApiSliceList.size()) {
        FullDb::CompeteSliceDomain curSlice = cannApiSliceList[index];
        // 是lmhead算子 说明一个模型的结束，此时保存模型信息
        if (CollectionUtil::IsEleInContainer(curSlice.name, lmHeadApiNameList)) {
            modelInfo.modelLayer = layer + 1;
            modelInfo.denseLayerList = denseLayerList;
            denseLayerList.clear();
            layer = -1;
            modelStage = "";
            index++;
            continue;
        }
        // 是excute的api，代表是一个新的层
        if (!CollectionUtil::IsEleInContainer(curSlice.name, layerExecuteApiNameList)) {
            return {};
        }
        if (modelStage.empty()) {
            modelStage = StringUtil::ContainsIgnoreCase(curSlice.name, "prefill") ? "prefill" : "decode";
        }
        index++;
        layer++;
        bool isDenseLayer = true;
        while (index < cannApiSliceList.size() && hardwareIndex < hardwareSliceList.size() &&
            CollectionUtil::IsEleInContainer(cannApiSliceList[index].name, groupedMatmulApiNameList)) {
            std::string key = modelStage + "_" + std::to_string(rankId) + "_" + std::to_string(layer);
            res[key].modelStage = modelStage;
            res[key].rankId = rankId;
            res[key].layer = static_cast<int>(NumberSafe::Sub(layer, denseLayerList.size()));
            res[key].visits += hardwareSliceList[hardwareIndex++].duration;
            res[key].version = "profiling";
            isDenseLayer = false;
            index++;
        }
        if (isDenseLayer) {
            denseLayerList.push_back(layer);
        }
    }
    return res;
}

bool ExpertHotspotManager::UpdateHeatMapFromProfiling(
    std::string &errorMsg, const std::string &clusterPath, const std::vector<std::string> &rankIdList) {
    if (clusterPath.empty() || rankIdList.empty()) {
        errorMsg = "Fail to update heatmap from profiling, invalid params.";
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    // 集群db不存在则直接返回
    if (database == nullptr) {
        errorMsg = "Fail to update heatmap from profiling, the cluster database not exist.";
        return false;
    }
    // 删除已有数据（后续优化，通过状态表来记录状态）
    if (!database->DeleteExpertHotspot("", "profiling")) {
        errorMsg = "Failed to clear old expert hotspot data, version: profiling.";
        return false;
    }
    ModelInfo modelInfo;
    FullDb::DataType dataType = Timeline::DataBaseManager::Instance().GetDataTypeByRank(rankIdList[0]);
    std::vector<std::string> cannApiList;
    cannApiList.insert(cannApiList.end(), layerExecuteApiNameList.begin(), layerExecuteApiNameList.end());
    cannApiList.insert(cannApiList.end(), groupedMatmulApiNameList.begin(), groupedMatmulApiNameList.end());
    cannApiList.insert(cannApiList.end(), lmHeadApiNameList.begin(), lmHeadApiNameList.end());
    for (const auto &rankId : rankIdList) {
        ExtractHeatMapParams params{rankId, dataType, cannApiList, groupedMatmulComputeNameList, clusterPath};
        if (!ExtractHeatMapFromTraceDb(params, modelInfo, errorMsg)) {
            return false;
        }
    }
    modelInfo.moeLayer = static_cast<int>(NumberSafe::Sub(modelInfo.modelLayer, modelInfo.denseLayerList.size()));
    modelInfo.expertNumber = modelInfo.rankNumber;
    ModelInfo curModelInfo = GetModelInfo(clusterPath);
    modelInfo.rankNumber = std::max(modelInfo.rankNumber, curModelInfo.rankNumber);
    modelInfo.expertNumber = std::max(modelInfo.expertNumber, curModelInfo.expertNumber);
    modelInfo.modelLayer = std::max(modelInfo.modelLayer, curModelInfo.modelLayer);
    SaveModelInfo(modelInfo, database);
    database->SaveExpertHotspot();
    return true;
}
}
}
}
