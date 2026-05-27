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
#include "SafeFile.h"
#include "JsonUtil.h"
#include "ClusterDef.h"
#include "RegexUtil.h"
#include "ExpertDeploymentParser.h"

namespace Dic::Module::Summary {
bool ExpertDeploymentParser::Parse(const std::string &filePath, const std::string &version) {
    if (db == nullptr) {
        return false;
    }
    // 解析文件名，获取模型阶段
    auto searchRes = RegexUtil::RegexSearch(filePath, EXPERT_DEPLOYMENT_FILE_REG);
    if (!searchRes.has_value() || searchRes.value().size() != regexMatchNumber) {
        return false;
    }
    std::string modelStage = searchRes.value()[1];
    // 打开文件
    std::ifstream expertDeployment = OpenReadFileSafely(filePath, std::ios::binary);
    if (!expertDeployment.good()) {
        return false;
    }
    // 读取文件内容
    std::string fileContent;
    std::copy(std::istream_iterator<unsigned char>(expertDeployment), std::istream_iterator<unsigned char>(),
        back_inserter(fileContent));
    if (fileContent.empty()) {
        return false;
    }
    // 将文件内容转换为json
    std::string errorMsg;
    std::optional<document_t> jsonInfoOpt = JsonUtil::TryParse(fileContent, errorMsg);
    if (!jsonInfoOpt.has_value()) {
        return false;
    }
    document_t &jsonInfo = jsonInfoOpt.value();
    // 数据读取内容读取，JsonUtil方法中都会对key值是否存在及数据类型进行校验
    modelInfoMap[modelStage].moeLayer = JsonUtil::GetInteger(jsonInfo, "moe_layer_count");
    if (!jsonInfo.HasMember("layer_list") || !jsonInfo["layer_list"].IsArray()) {
        // 判断layer_list的数据是否存在，是否为数组
        return false;
    }
    std::vector<ExpertDeploymentStruct> res;
    for (const auto &item : jsonInfo["layer_list"].GetArray()) {
        int layerId = JsonUtil::GetInteger(item, "layer_id");
        modelInfoMap[modelStage].rankNumber = JsonUtil::GetInteger(item, "device_count");
        if (!item.HasMember("device_list") || !item["device_list"].IsArray()) {
            return false;
        }
        for (const auto &deviceInfo : item["device_list"].GetArray()) {
            int deviceId = JsonUtil::GetInteger(deviceInfo, "device_id");
            std::vector<int> expertList = JsonUtil::GetVector<int>(deviceInfo, "device_expert");
            if (expertList.empty()) {
                return false;
            }
            db->InsertExpertDeploymentForCache({deviceId, expertList, layerId, modelStage, version});
            modelInfoMap[modelStage].expertNumber = std::max(
                modelInfoMap[modelStage].expertNumber, *std::max_element(expertList.begin(), expertList.end()) + 1);
        }
    }
    return true;
}

std::map<std::string, ModelInfo> ExpertDeploymentParser::GetModelInfoMap() { return modelInfoMap; }
}
