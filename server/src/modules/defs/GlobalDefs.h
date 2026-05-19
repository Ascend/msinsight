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

#ifndef DATA_INSIGHT_CORE_DEFS_H
#define DATA_INSIGHT_CORE_DEFS_H

#include "rapidjson.h"
#include "document.h"
#include "writer.h"
#include "stringbuffer.h"
#include "unordered_map"
#include "JsonUtil.h"
#include <vector>
#include <algorithm>
#include <string>

namespace Dic {
using json_t = rapidjson::Value;
using document_t = rapidjson::Document;
using namespace rapidjson;

enum class ParserType {
    DB = 0,
    BIN = 1,
    JSON = 2,
    IE = 4,
    DB_NPUMONITOR = 5,
    ACLGRPAH_DEBUG_JSON = 6,
    DB_MEMSCOPE = 7,
    PKL_MEM_SNAPSHOT = 8,
    TRITON_MEMORY = 9,
    DB_FTRACE = 10,
    OTHER = 20
};

inline std::string CastParserTypeToStr(ParserType type)
{
    switch (type) {
        case ParserType::DB:
            return "DB";
        case ParserType::BIN:
            return "BIN";
        case ParserType::JSON:
            return "JSON";
        case ParserType::OTHER:
            return "OTHER";
        default:
            return "";
    }
    return "";
}
enum class ProjectTypeEnum {
    DB = 0,
    BIN = 1,
    IPYNB = 2,
    TEXT_CLUSTER = 3,
    SIMULATION = 4,
    TRACE = 5,
    DB_CLUSTER = 6,
    IE = 7,
    DB_NPUMONITOR = 8,
    ACLGRAPH_DEBUG = 9,
    DB_MEMSCOPE = 10,
    PKL_MEM_SNAPSHOT = 11,
    TRITON = 12,
    DB_FTRACE = 13,
    OTHER = 100
};

inline std::vector<ProjectTypeEnum> projectTypeSupportCompare = {
    ProjectTypeEnum::DB,
    ProjectTypeEnum::DB_NPUMONITOR,
    ProjectTypeEnum::TEXT_CLUSTER,
    ProjectTypeEnum::SIMULATION,
    ProjectTypeEnum::TRACE,
    ProjectTypeEnum::DB_CLUSTER,
    ProjectTypeEnum::BIN,
};

static inline bool IsSupportCompareType(ProjectTypeEnum projectTypeEnum)
{
    auto it = std::find(projectTypeSupportCompare.begin(), projectTypeSupportCompare.end(),
                        projectTypeEnum);
    if (it != projectTypeSupportCompare.end()) {
        return true;
    }
    return false;
}

inline std::unordered_map<ProjectTypeEnum, uint8_t> projectTypeGroup = {
    {ProjectTypeEnum::DB, 1},
    {ProjectTypeEnum::TEXT_CLUSTER, 2},
    {ProjectTypeEnum::SIMULATION, 3},
    {ProjectTypeEnum::TRACE, 2},
    {ProjectTypeEnum::DB_CLUSTER, 1},
    {ProjectTypeEnum::DB_NPUMONITOR, 1}
};

static inline ParserType coverProjectTypeToParserType(ProjectTypeEnum projectTypeEnum)
{
    switch (projectTypeEnum) {
        case ProjectTypeEnum::DB_CLUSTER:
            return ParserType::DB;
        case ProjectTypeEnum::DB:
            return ParserType::DB;
        case ProjectTypeEnum::DB_NPUMONITOR:
            return ParserType::DB_NPUMONITOR;
        case ProjectTypeEnum::BIN:
            return ParserType::BIN;
        case ProjectTypeEnum::IE:
            return ParserType::IE;
        case ProjectTypeEnum::ACLGRAPH_DEBUG:
            return ParserType::ACLGRPAH_DEBUG_JSON;
        case ProjectTypeEnum::DB_MEMSCOPE:
            return ParserType::DB_MEMSCOPE;
        case ProjectTypeEnum::PKL_MEM_SNAPSHOT:
            return ParserType::PKL_MEM_SNAPSHOT;
        case ProjectTypeEnum::TRITON:
            return ParserType::TRITON_MEMORY;
        case ProjectTypeEnum::DB_FTRACE:
            return ParserType::DB_FTRACE;
        default:
            return ParserType::JSON;
    }
}

static inline bool isFileConflict(ProjectTypeEnum oldProjectType, ProjectTypeEnum newProjectType)
{
    if (projectTypeGroup.count(oldProjectType) == 0 || projectTypeGroup.count(newProjectType) == 0) {
        return true;
    }
    if (projectTypeGroup[oldProjectType] == projectTypeGroup[newProjectType]) {
        return false;
    }
    return true;
}

static inline bool IsComparable(const ProjectTypeEnum &baselineProjectType, const ProjectTypeEnum &curProjectType)
{
    if (baselineProjectType == curProjectType) {
        return true;
    }
    return !isFileConflict(baselineProjectType, curProjectType);
}

struct RankInfo {
    RankInfo() = default;
    RankInfo(std::string cluster, std::string host, std::string rankId, std::string deviceId, std::string rankName)
        :cluster(std::move(cluster)),
        host(std::move(host)),
        rankId(std::move(rankId)),
        deviceId(std::move(deviceId)),
        rankName(std::move(rankName))
    {
    }

    json_t SerializationToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const
    {
        json_t rankInfo(kObjectType) ;
        JsonUtil::AddMember(rankInfo, "clusterId", cluster, allocator);
        JsonUtil::AddMember(rankInfo, "host", host, allocator);
        JsonUtil::AddMember(rankInfo, "rankId", rankId, allocator);
        JsonUtil::AddMember(rankInfo, "rankName", rankName, allocator);
        JsonUtil::AddMember(rankInfo, "deviceId", deviceId, allocator);
        return rankInfo;
    }
    std::string cluster;
    std::string host;
    std::string rankId;
    std::string deviceId;
    std::string rankName;
};

struct RankEntry {
    RankEntry() = default;
    RankEntry(std::string fileId, std::string rankId, std::string parseFolder)
        :fileId(std::move(fileId)), rankId(std::move(rankId)), parseFolder(std::move(parseFolder))
    {}
    std::string fileId;
    std::string rankId;
    std::string deviceId;
    std::vector<RankInfo> rankInfo;  // rankInfo：cluster + host + rankId + deviceId
    std::string parseFolder;
    std::vector<std::string> parseFileList;
    bool isDevice{false};
    int64_t projectType = static_cast<int64_t>(ProjectTypeEnum::OTHER);
};
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_DEFS_H