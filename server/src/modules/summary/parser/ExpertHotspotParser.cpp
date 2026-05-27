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
#include "ExpertHotspotParser.h"
#include "SafeFile.h"
#include "NumberSafeUtil.h"
#include "NumberUtil.h"
#include "RegexUtil.h"

namespace Dic::Module::Summary {
bool ExpertHotspotParser::Parse(const std::string &filePath, const std::string &version) {
    if (db == nullptr) {
        return false;
    }
    // 解析文件名，获取模型阶段和rankId
    auto searchRes = RegexUtil::RegexSearch(filePath, EXPERT_HOTSPOT_FILE_REG);
    if (!searchRes.has_value() || searchRes.value().size() != regexMatchNumber) {
        return false;
    }

    // 读取文件，读取过程中会校验文件状态
    std::ifstream file = OpenReadFileSafely(filePath);
    std::string lineRead;
    int rankId = NumberUtil::StringToInt(searchRes.value()[2]);
    std::string modelStage = searchRes.value()[1];
    int layer = 0;
    size_t expertCountPerRank = 0;
    std::vector<std::string> lines;
    while (std::getline(file, lineRead)) {
        lines.push_back(lineRead);
        // 数据来源处已校验moe layer不可能小于0
        if (lines.size() > config.moeLayer) {
            lines.erase(lines.begin());
        }
    }
    for (const auto &line : lines) {
        std::vector<std::string> row = StringUtil::StringSplit(line);
        if (expertCountPerRank == 0) {
            // 读取的第一行数据作为每个rank的专家数量
            expertCountPerRank = row.size();
        }
        if (expertCountPerRank != row.size()) {
            Server::ServerLog::Warn(
                "Invalid row size, model type:", modelStage, ", rank id:", rankId, ", row number:", layer);
        }
        for (size_t i = 0; i < row.size(); ++i) {
            ExpertHotspotStruct expertHotspot{
                modelStage, rankId, NumberUtil::StringToUnsignedLongLong(row[i]), layer, static_cast<int>(i), version};
            db->InsertExpertHotspotDataForCache(expertHotspot);
        }
        layer++;
    }
    modelInfoMap[modelStage].rankNumber = std::max(modelInfoMap[modelStage].rankNumber, rankId + 1);
    modelInfoMap[modelStage].moeLayer = layer;
    // 单独看热点数据，没有专家id，因此使用专家数量作为专家id的最大值
    uint64_t expertNumber = NumberSafe::Muls(modelInfoMap[modelStage].rankNumber, expertCountPerRank);
    modelInfoMap[modelStage].expertNumber =
        static_cast<int>(NumberUtil::CeilingClamp(expertNumber, static_cast<uint64_t>(INT_MAX)));
    return true;
}

std::map<std::string, ModelInfo> ExpertHotspotParser::GetModelInfoMap() { return modelInfoMap; }
}
