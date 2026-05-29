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

#include "pch.h"
#include "ParserStatusManager.h"
#include "TextClusterDatabase.h"
#include "CommunicationRapidSaxHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
CommunicationRapidSaxHandler::CommunicationRapidSaxHandler(
    std::shared_ptr<TextClusterDatabase> database, const std::string &uniqueKey)
    : uniqueKey(uniqueKey) {
    this->database = database;
}

CommunicationRapidSaxHandler::~CommunicationRapidSaxHandler() {}

bool CommunicationRapidSaxHandler::Null() { return true; }

bool CommunicationRapidSaxHandler::Bool(bool b) { return true; }

bool CommunicationRapidSaxHandler::Int(int i) { return true; }

bool CommunicationRapidSaxHandler::Uint(unsigned int u) { return true; }

bool CommunicationRapidSaxHandler::Int64(int64_t i) { return true; }

bool CommunicationRapidSaxHandler::Uint64(uint64_t u) { return true; }

bool CommunicationRapidSaxHandler::Double(double d) { return true; }

bool CommunicationRapidSaxHandler::RawNumber(const char *str, SizeType len, bool copy) {
    if (str == nullptr) {
        ServerLog::Error("Parsing communication.json encounters nullptr.");
        return false;
    }
    std::string s(str, len);
    if (currentDepth == sizeDistributionDepth) {
        bandwidth.sizeDistribution += s + ",";
        return true;
    }
    if (currentDepth == tableFlagDepth + 1 && tableFlag == "Communication Time Info") {
        if (currentKey == "Start Timestamp(us)") {
            int64_t tempStartTime = NumberUtil::TimestampUsToNs(s);
            timeInfo.startTime = NumberUtil::Int64ToUint64(tempStartTime);
        }
        if (currentKey == "Elapse Time(ms)") {
            timeInfo.elapseTime = NumberUtil::StringToDouble(s);
        }
        if (currentKey == "Idle Time(ms)") {
            timeInfo.idleTime = NumberUtil::StringToDouble(s);
        }
        if (currentKey == "Synchronization Time Ratio") {
            timeInfo.synchronizationTimeRatio = NumberUtil::StringToDouble(s);
        }
        if (currentKey == "Synchronization Time(ms)") {
            timeInfo.synchronizationTime = NumberUtil::StringToDouble(s);
        }
        if (currentKey == "Transit Time(ms)") {
            timeInfo.transitTime = NumberUtil::StringToDouble(s);
        }
        if (currentKey == "Wait Time Ratio") {
            timeInfo.waitTimeRatio = NumberUtil::StringToDouble(s);
        }
        if (currentKey == "Wait Time(ms)") {
            timeInfo.waitTime = NumberUtil::StringToDouble(s);
        }
    }
    if (currentDepth == infoDepth + 1 && tableFlag == "Communication Bandwidth Info") {
        if (currentKey == "Bandwidth(GB/s)") {
            bandwidth.bandwidthSize = NumberUtil::StringToDouble(s);
        }
        if (currentKey == "Transit Size(MB)") {
            bandwidth.transitSize = NumberUtil::StringToDouble(s);
        }
        if (currentKey == "Transit Time(ms)") {
            bandwidth.transitTime = NumberUtil::StringToDouble(s);
        }
    }
    return true;
}

bool CommunicationRapidSaxHandler::String(const char *str, rapidjson::SizeType length, bool copy) { return true; }

bool CommunicationRapidSaxHandler::StartObject() {
    currentDepth++;
    if (currentDepth == sizeDistributionDepth) {
        bandwidth.sizeDistribution = "{";
    }
    return true;
}

bool CommunicationRapidSaxHandler::Key(const char *str, rapidjson::SizeType length, bool copy) {
    currentKey = str;
    if (currentDepth == stageIdDepth) {
        if (currentKey == "p2p") {
            stageId = str;
        } else {
            std::vector<std::string> rankList = StringUtil::SplitStringWithParenthesesByComma(str);
            stageId = StringUtil::JoinNumberStrWithParenthesesByOrder(rankList);
        }
    }
    if (currentDepth == stepIdDepth) {
        stepId = str;
    }
    if (currentDepth == tempOpNameDepth) {
        tempOpName = str;
    }
    if (currentDepth == rankIdDepth) {
        rankId = str;
    }
    if (currentDepth == tableFlagDepth) {
        tableFlag = str;
    }
    if (currentDepth == infoDepth && std::strcmp("Communication Bandwidth Info", tableFlag.c_str()) == 0) {
        transportType = str;
    }
    if (currentDepth == sizeDistributionDepth) {
        bandwidth.sizeDistribution += "\"" + currentKey + "\"";
    }
    return true;
}

std::string CommunicationRapidSaxHandler::GenerateTimeInfoKey(const CommunicationTimeInfo &info) const {
    return StringUtil::FormatString("{}_{}_{}", info.iterationId, info.opSuffix, info.rankId);
}

void CommunicationRapidSaxHandler::StatTimeTotalOpInfo(const CommunicationTimeInfo &info) {
    if (info.opSuffix == "" || info.opName == "Total Op Info") {
        return;
    }
    std::string key = GenerateTimeInfoKey(info);
    if (timeOpTotalInfoMap.find(key) != timeOpTotalInfoMap.end()) {
        timeOpTotalInfoMap[key].elapseTime += info.elapseTime;
        timeOpTotalInfoMap[key].synchronizationTime += info.synchronizationTime;
        timeOpTotalInfoMap[key].transitTime += info.transitTime;
        timeOpTotalInfoMap[key].waitTime += info.waitTime;
        timeOpTotalInfoMap[key].idleTime += info.idleTime;
        timeOpTotalInfoMap[key].synchronizationTimeRatio =
            NumberUtil::DoubleReservedNDigits(timeOpTotalInfoMap[key].synchronizationTime /
                    (timeOpTotalInfoMap[key].synchronizationTime + timeOpTotalInfoMap[key].transitTime),
                4);
        timeOpTotalInfoMap[key].waitTimeRatio = NumberUtil::DoubleReservedNDigits(
            timeOpTotalInfoMap[key].waitTime / (timeOpTotalInfoMap[key].waitTime + timeOpTotalInfoMap[key].transitTime),
            4);
    } else {
        timeOpTotalInfoMap[key] = info;
        timeOpTotalInfoMap[key].opName = "Total Op Info";
        timeOpTotalInfoMap[key].startTime = 0;
    }
}

std::string CommunicationRapidSaxHandler::GenerateBandwidthInfoKey(const CommunicationBandWidth &info) const {
    return StringUtil::FormatString("{}_{}_{}_{}", info.iterationId, info.rankId, info.opSuffix, info.transportType);
}

void CommunicationRapidSaxHandler::StatBandwidthTotalOpInfo(const CommunicationBandWidth &info) {
    if (info.opSuffix == "" || info.opName == "Total Op Info") {
        return;
    }
    std::string key = GenerateBandwidthInfoKey(info);
    if (bandwidthOpTotalInfoMap.find(key) != bandwidthOpTotalInfoMap.end()) {
        bandwidthOpTotalInfoMap[key].transitSize += info.transitSize;
        bandwidthOpTotalInfoMap[key].transitTime += info.transitTime;
        if (bandwidthOpTotalInfoMap[key].transitTime != 0) {
            bandwidthOpTotalInfoMap[key].bandwidthSize = NumberUtil::DoubleReservedNDigits(
                bandwidthOpTotalInfoMap[key].transitSize / bandwidthOpTotalInfoMap[key].transitTime, 4);
        }
        for (const auto &item : info.packageMap) {
            if (bandwidthOpTotalInfoMap[key].packageMap.find(item.first) !=
                bandwidthOpTotalInfoMap[key].packageMap.end()) {
                bandwidthOpTotalInfoMap[key].packageMap[item.first].packageNumber += item.second.packageNumber;
                bandwidthOpTotalInfoMap[key].packageMap[item.first].packageTime += item.second.packageTime;
            } else {
                bandwidthOpTotalInfoMap[key].packageMap[item.first] = item.second;
            }
        }
    } else {
        bandwidthOpTotalInfoMap[key] = info;
        bandwidthOpTotalInfoMap[key].opName = "Total Op Info";
    }
}

std::unordered_map<std::string, PackageInfo> CommunicationRapidSaxHandler::TransStrToPackageMap(
    const std::string &str) {
    std::string error;
    auto data = JsonUtil::TryParse<kParseNumbersAsStringsFlag>(str, error);
    if (!data.has_value() || !data->IsObject()) {
        ServerLog::Error("Failed to transfer size distribution data to package map.");
        return {};
    }
    std::unordered_map<std::string, PackageInfo> res;
    for (const auto &item : data->GetObj()) {
        if (!item.name.IsString() || !item.value.IsArray()) {
            continue;
        }
        std::string packageSizeStr = item.name.GetString();
        std::vector<double> valueList;
        for (const auto &num : item.value.GetArray()) {
            double temp = num.IsString() ? NumberUtil::StringToDouble(num.GetString()) : 0;
            valueList.push_back(temp);
        }
        if (valueList.size() != 2) {
            continue;
        }
        res[packageSizeStr] = {valueList[0], valueList[1]};
    }
    return res;
}

std::string CommunicationRapidSaxHandler::TransPackageMapToStr(
    std::unordered_map<std::string, PackageInfo> &packageMap) {
    std::vector<std::string> packageStrList;
    for (const auto &item : packageMap) {
        std::string itemStr = StringUtil::FormatString("\"{}\":[{},{}]", item.first,
            std::to_string(item.second.packageNumber), std::to_string(item.second.packageTime));
        packageStrList.push_back(itemStr);
    }
    return "{" + StringUtil::join(packageStrList, ",") + "}";
}

bool CommunicationRapidSaxHandler::EndObject(rapidjson::SizeType memberCount) {
    if (ParserStatusManager::Instance().IsClusterParserFinalState(uniqueKey)) {
        return false;
    }
    // 获取所有的groupId映射关系
    if (database == nullptr) {
        ServerLog::Error("Can't get cluster database for read when parse communication data.");
        return false;
    }
    if (groupIdsMap.empty()) {
        InitGroupInfoMap();
    }
    if (database == nullptr) {
        ServerLog::Error("Can't get cluster database for write when parse communication data.");
        return false;
    }
    currentDepth--;
    DealData();
    return true;
}

void CommunicationRapidSaxHandler::DealData() {
    if (currentDepth == infoDepth && std::strcmp(tableFlag.c_str(), "Communication Bandwidth Info") == 0) {
        GetBandwidth();
        if (bandwidth.opSuffix != "") {
            database->InsertBandwidth(bandwidth);
        } else {
            isOldData = true;
        }
        StatBandwidthTotalOpInfo(bandwidth);
        bandwidth = CommunicationBandWidth{};
    }

    if (currentDepth == tableFlagDepth && std::strcmp(tableFlag.c_str(), "Communication Time Info") == 0) {
        GetTimeInfo();
        if (timeInfo.opSuffix != "") {
            database->InsertTimeInfo(timeInfo);
        } else {
            isOldData = true;
        }
        StatTimeTotalOpInfo(timeInfo);
        timeInfo = CommunicationTimeInfo{};
    }
    if (currentDepth == infoDepthSeven) {
        if (StringUtil::EndWith(bandwidth.sizeDistribution, ",")) {
            bandwidth.sizeDistribution.resize(bandwidth.sizeDistribution.size() - 1);
        }
        bandwidth.sizeDistribution += "}";
        bandwidth.packageMap = TransStrToPackageMap(bandwidth.sizeDistribution);
    }
    if (currentDepth == 0) {
        if (isOldData) {
            for (auto &item : timeOpTotalInfoMap) {
                database->InsertTimeInfo(item.second);
            }
            for (auto &item : bandwidthOpTotalInfoMap) {
                item.second.sizeDistribution = TransPackageMapToStr(item.second.packageMap);
                database->InsertBandwidth(item.second);
            }
        }
        if (!SaveGroupInfoMap()) {
            ServerLog::Error("Fail to insert duplicate update group info when parse communication data.");
        }
    }
}

bool CommunicationRapidSaxHandler::StartArray() {
    bandwidth.sizeDistribution += ":[";
    return true;
}

bool CommunicationRapidSaxHandler::EndArray(rapidjson::SizeType elementCount) {
    if (currentDepth == sizeDistributionDepth) {
        if (StringUtil::EndWith(bandwidth.sizeDistribution, ",")) {
            bandwidth.sizeDistribution.resize(bandwidth.sizeDistribution.size() - 1);
        }
        bandwidth.sizeDistribution += "],";
    }
    return true;
}

void CommunicationRapidSaxHandler::GetBandwidth() {
    bandwidth.iterationId = stepId.length() > stepSubLen ? stepId.substr(stepSubLen) : stepId;
    if (std::strcmp(stepId.c_str(), "step") == 0) {
        bandwidth.iterationId = "0";
    }
    bandwidth.rankId = rankId;
    size_t index = tempOpName.find_last_of('@');
    if (index != std::string::npos) {
        bandwidth.opName = tempOpName.substr(0, index);
        bandwidth.opSuffix = tempOpName.substr(index + 1);
    } else {
        bandwidth.opName = tempOpName;
    }
    bandwidth.transportType = transportType;
    bandwidth.stageId = GenerateAndGetGroupInfoId(stageId, bandwidth.opSuffix);
}

void CommunicationRapidSaxHandler::GetTimeInfo() {
    timeInfo.rankId = rankId;
    // 去掉step前缀
    timeInfo.iterationId = stepId.length() > stepSubLen ? stepId.substr(stepSubLen) : stepId;
    if (std::strcmp(stepId.c_str(), "step") == 0) {
        timeInfo.iterationId = "0";
    }
    size_t index = tempOpName.find_last_of('@');
    if (index != std::string::npos) {
        timeInfo.opName = tempOpName.substr(0, index);
        timeInfo.opSuffix = tempOpName.substr(index + 1);
    } else {
        timeInfo.opName = tempOpName;
    }
    timeInfo.stageId = GenerateAndGetGroupInfoId(stageId, timeInfo.opSuffix);
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
