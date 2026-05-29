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
#include "CommunicationMatrixRapidHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
CommunicationMatrixRapidHandler::CommunicationMatrixRapidHandler(
    std::shared_ptr<TextClusterDatabase> database, const std::string &uniqueKey)
    : uniqueKey(uniqueKey) {
    this->database = database;
    currentObject.SetObject();
}

CommunicationMatrixRapidHandler::~CommunicationMatrixRapidHandler() {}

bool CommunicationMatrixRapidHandler::Null() { return true; }

bool CommunicationMatrixRapidHandler::Bool(bool bl) {
    rapidjson::Value tempKey(currentKey.c_str(), currentObject.GetAllocator());
    currentObject.AddMember(tempKey, bl, currentObject.GetAllocator());
    return true;
}

bool CommunicationMatrixRapidHandler::Int(int i) {
    rapidjson::Value tempKey(currentKey.c_str(), currentObject.GetAllocator());
    currentObject.AddMember(tempKey, i, currentObject.GetAllocator());
    return true;
}

bool CommunicationMatrixRapidHandler::Uint(unsigned int uint) {
    rapidjson::Value tempKey(currentKey.c_str(), currentObject.GetAllocator());
    currentObject.AddMember(tempKey, uint, currentObject.GetAllocator());
    return true;
}

bool CommunicationMatrixRapidHandler::Int64(int64_t i) {
    rapidjson::Value tempKey(currentKey.c_str(), currentObject.GetAllocator());
    currentObject.AddMember(tempKey, i, currentObject.GetAllocator());
    return true;
}

bool CommunicationMatrixRapidHandler::Uint64(uint64_t u) {
    rapidjson::Value tempKey(currentKey.c_str(), currentObject.GetAllocator());
    currentObject.AddMember(tempKey, u, currentObject.GetAllocator());
    return true;
}

bool CommunicationMatrixRapidHandler::Double(double doubleVal) {
    rapidjson::Value tempKey(currentKey.c_str(), currentObject.GetAllocator());
    currentObject.AddMember(tempKey, doubleVal, currentObject.GetAllocator());
    return true;
}

bool CommunicationMatrixRapidHandler::String(const char *str, rapidjson::SizeType length, bool copy) {
    rapidjson::Value tempKey(currentKey.c_str(), currentObject.GetAllocator());
    rapidjson::Value val(str, currentObject.GetAllocator());
    currentObject.AddMember(tempKey, val, currentObject.GetAllocator());
    return true;
}

bool CommunicationMatrixRapidHandler::StartObject() {
    currentDepth++;
    return true;
}

bool CommunicationMatrixRapidHandler::Key(const char *str, rapidjson::SizeType length, bool copy) {
    currentKey = str;
    if (currentDepth == groupDepth) {
        // groupId内容可能为乱序，需要重新进行排序
        if (currentKey == "p2p") {
            groupId = str;
        } else {
            std::vector<std::string> rankList = StringUtil::SplitStringWithParenthesesByComma(str);
            groupId = StringUtil::JoinNumberStrWithParenthesesByOrder(rankList);
        }
    }
    if (currentDepth == stepDepth) {
        iterationId = str;
    }
    if (currentDepth == opNameDepth) {
        tempOpName = str;
    }
    if (currentDepth == ranksDepth) {
        tempRank = str;
    }
    return true;
}

std::string CommunicationMatrixRapidHandler::GenerateMatrixKey(const CommunicationMatrixInfo &matrixInfo) const {
    return StringUtil::FormatString("{}_{}_{}_{}_{}", matrixInfo.iterationId, std::to_string(matrixInfo.srcRank),
        std::to_string(matrixInfo.dstRank), matrixInfo.transportType, matrixInfo.groupName);
}

void CommunicationMatrixRapidHandler::StatTotalOpInfo(const CommunicationMatrixInfo &matrixInfo) {
    if (matrixInfo.groupName == "" || matrixInfo.sortOp == "Total Op Info" ||
        !StringUtil::Contains(matrixInfo.sortOp, "total")) {
        return;
    }
    std::string key = GenerateMatrixKey(matrixInfo);
    if (matrixTotalOpInfoMap.find(key) != matrixTotalOpInfoMap.end()) {
        matrixTotalOpInfoMap[key].transitSize += matrixInfo.transitSize;
        matrixTotalOpInfoMap[key].transitTime += matrixInfo.transitTime;
        matrixTotalOpInfoMap[key].bandwidth = NumberUtil::DoubleReservedNDigits(
            matrixTotalOpInfoMap[key].transitSize / matrixTotalOpInfoMap[key].transitTime, 4);
    } else {
        matrixTotalOpInfoMap[key] = {matrixInfo.groupId, matrixInfo.iterationId, "Total Op Info", "",
            matrixInfo.groupName, matrixInfo.srcRank, matrixInfo.dstRank, matrixInfo.transportType,
            matrixInfo.transitSize, matrixInfo.transitTime, matrixInfo.bandwidth};
    }
}

bool CommunicationMatrixRapidHandler::EndObject(rapidjson::SizeType memberCount) {
    if (ParserStatusManager::Instance().IsClusterParserFinalState(uniqueKey)) {
        return false;
    }
    if (database == nullptr) {
        ServerLog::Error("Can't get cluster database.");
        return false;
    }
    if (groupIdsMap.empty()) {
        InitGroupInfoMap();
    }
    currentDepth--;
    if (currentDepth == ranksDepth) {
        CommunicationMatrixInfo matrix = MapToMatrixInfo(currentObject);
        if (matrix.groupName != "") {
            database->InsertCommunicationMatrix(matrix);
        } else {
            isOldData = true;
        }
        StatTotalOpInfo(matrix);
        currentObject.RemoveAllMembers();
    }
    if (currentDepth == 0) {
        if (isOldData) {
            for (auto &item : matrixTotalOpInfoMap) {
                database->InsertCommunicationMatrix(item.second);
            }
        }
        if (!SaveGroupInfoMap()) {
            ServerLog::Error("Fail to insert duplicate update group info when parse communication matrix data.");
        }
    }
    return true;
}

bool CommunicationMatrixRapidHandler::StartArray() { return true; }

bool CommunicationMatrixRapidHandler::EndArray(rapidjson::SizeType elementCount) { return true; }

CommunicationMatrixInfo CommunicationMatrixRapidHandler::MapToMatrixInfo(const rapidjson::Document &json) {
    CommunicationMatrixInfo matrixInfo;
    matrixInfo.iterationId = iterationId;
    matrixInfo.iterationId = iterationId.length() > stepSubLen ? iterationId.substr(stepSubLen) : iterationId;
    if (std::strcmp(iterationId.c_str(), "step") == 0) {
        matrixInfo.iterationId = "0";
    }
    size_t nameIndex = tempOpName.empty() ? 0 : tempOpName.find_last_of('@');
    if (nameIndex > 0 && nameIndex != std::string::npos) {
        matrixInfo.sortOp = tempOpName.substr(0, nameIndex);
        matrixInfo.groupName = tempOpName.substr(nameIndex + 1);
    } else {
        matrixInfo.sortOp = tempOpName;
    }
    size_t rankIndex = tempRank.empty() ? 0 : tempRank.find_last_of('-');
    if (rankIndex > 0 && rankIndex != std::string::npos) {
        matrixInfo.srcRank = NumberUtil::StringToInt(tempRank.substr(0, rankIndex));
        matrixInfo.dstRank = NumberUtil::StringToInt(tempRank.substr(rankIndex + 1));
    } else {
        matrixInfo.srcRank = NumberUtil::StringToInt(tempRank);
    }
    matrixInfo.transportType = JsonUtil::GetDumpString(json, "Transport Type");
    matrixInfo.transitTime = JsonUtil::GetDouble(json, "Transit Time(ms)");
    matrixInfo.transitSize = JsonUtil::GetDouble(json, "Transit Size(MB)");
    matrixInfo.bandwidth = JsonUtil::GetDouble(json, "Bandwidth(GB/s)");
    matrixInfo.opName = JsonUtil::GetString(json, "Op Name");
    matrixInfo.groupId = GenerateAndGetGroupInfoId(groupId, matrixInfo.groupName);
    return matrixInfo;
}

} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
