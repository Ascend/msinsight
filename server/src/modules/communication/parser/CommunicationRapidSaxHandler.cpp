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
#include <cmath>
#include <cerrno>
#include <cstdlib>
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
    char *end = nullptr;
    errno = 0;
    const double value = std::strtod(s.c_str(), &end);
    if (errno != 0 || end == s.c_str() || end != s.c_str() + s.size() || !std::isfinite(value)) {
        return true;
    }
    if (currentDepth == tableFlagDepth + 1 && tableFlag == "Communication Time Info") {
        if (currentKey == "Start Timestamp(us)") {
            int64_t tempStartTime = NumberUtil::TimestampUsToNs(s);
            timeInfo.startTime = tempStartTime < 0 ? -1 : tempStartTime;
        }
        if (currentKey == "Elapse Time(ms)") {
            timeInfo.elapseTime = value;
        }
        if (currentKey == "Idle Time(ms)") {
            timeInfo.idleTime = value;
        }
        if (currentKey == "Synchronization Time Ratio") {
            timeInfo.synchronizationTimeRatio = value;
        }
        if (currentKey == "Synchronization Time(ms)") {
            timeInfo.synchronizationTime = value;
        }
        if (currentKey == "Transit Time(ms)") {
            timeInfo.transitTime = value;
        }
        if (currentKey == "Wait Time Ratio") {
            timeInfo.waitTimeRatio = value;
        }
        if (currentKey == "Wait Time(ms)") {
            timeInfo.waitTime = value;
        }
    }
    if (currentDepth == infoDepth + 1 && tableFlag == "Communication Bandwidth Info") {
        if (currentKey == "Large Packet Ratio") {
            bandwidth.largePackageRatio = value;
        }
        if (currentKey == "Bandwidth(GB/s)") {
            bandwidth.bandwidthSize = value;
        }
        if (currentKey == "Transit Size(MB)") {
            bandwidth.transitSize = value;
        }
        if (currentKey == "Transit Time(ms)") {
            bandwidth.transitTime = value;
        }
    }
    return true;
}

bool CommunicationRapidSaxHandler::String(const char *str, rapidjson::SizeType length, bool copy) {
    if ((currentDepth == tableFlagDepth + 1 && tableFlag == "Communication Time Info") ||
        (currentDepth == infoDepth + 1 && tableFlag == "Communication Bandwidth Info")) {
        return RawNumber(str, length, copy);
    }
    return true;
}

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
    if (currentDepth == 0 && !SaveGroupInfoMap()) {
        ServerLog::Error("Fail to insert duplicate update group info when parse communication data.");
    }
    return true;
}

void CommunicationRapidSaxHandler::DealData() {
    if (currentDepth == infoDepth && std::strcmp(tableFlag.c_str(), "Communication Bandwidth Info") == 0) {
        GetBandwidth();
        database->InsertBandwidth(bandwidth);
        bandwidth = CommunicationBandWidth{};
    }

    if (currentDepth == tableFlagDepth && std::strcmp(tableFlag.c_str(), "Communication Time Info") == 0) {
        GetTimeInfo();
        database->InsertTimeInfo(timeInfo);
        timeInfo = CommunicationTimeInfo{};
    }
    if (currentDepth == infoDepthSeven) {
        if (StringUtil::EndWith(bandwidth.sizeDistribution, ",")) {
            bandwidth.sizeDistribution.resize(bandwidth.sizeDistribution.size() - 1);
        }
        bandwidth.sizeDistribution += "}";
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
