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
#include "SourceFileParser.h"
#include "RooflineParserImpl.h"
namespace Dic::Module::Source {
using namespace Server;
using namespace Protocol;
bool RooflineParserImpl::GetDetailsRoofline(const std::string &jsonStr, Protocol::DetailsRooflineBody &responseBody) {
    std::optional<Protocol::RooflineData> rooflineData = ConvertStrToRooflineData(jsonStr);
    if (rooflineData.has_value()) {
        responseBody.advice = rooflineData->advice;
        responseBody.data = std::move(rooflineData->multipleRooflines);
        return true;
    }
    return false;
}

std::optional<Protocol::RooflineData> RooflineParserImpl::ConvertStrToRooflineData(const std::string &json) {
    if (json.empty()) {
        return std::nullopt;
    }
    Protocol::RooflineData rooflineData;
    std::string error;
    auto jsonParsed = JsonUtil::TryParse<kParseNumbersAsStringsFlag>(json, error);
    if (!error.empty()) {
        ServerLog::Error("Illegal roofline data which can't parse into json, error:", error);
        return std::nullopt;
    }
    rooflineData.advice = JsonUtil::GetString(jsonParsed.value(), "advice");
    if (!jsonParsed.value().HasMember("multiple_rooflines")) {
        ServerLog::Error("Not have roofline graph part");
        return std::nullopt;
    }
    Value &multiRoofline = jsonParsed.value()["multiple_rooflines"];
    if (!multiRoofline.IsArray()) {
        ServerLog::Error("Invalid data format, roofline graph should be array");
        return std::nullopt;
    }
    std::transform(multiRoofline.GetArray().begin(), multiRoofline.GetArray().end(),
        std::back_inserter(rooflineData.multipleRooflines), RooflineParserImpl::ParseRooflineData);
    return rooflineData;
}

Protocol::RooflineGraph RooflineParserImpl::ParseRooflineData(const json_t &item) {
    Protocol::RooflineGraph rooflineData;
    rooflineData.title = JsonUtil::GetString(item, "title");
    if (!item.HasMember("rooflines")) {
        ServerLog::Warn("Not have roofline data");
        return rooflineData;
    }
    const Value &rooflines = item["rooflines"];
    if (!rooflines.IsArray()) {
        return rooflineData;
    }
    std::transform(rooflines.GetArray().begin(), rooflines.GetArray().end(), std::back_inserter(rooflineData.rooflines),
        RooflineParserImpl::ParseRoofline);
    return rooflineData;
}

Protocol::Roofline RooflineParserImpl::ParseRoofline(const json_t &item) {
    Protocol::Roofline roofline;
    roofline.bw = JsonUtil::GetString(item, "bw");
    roofline.bwName = JsonUtil::GetString(item, "bw_name");
    roofline.computility = JsonUtil::GetString(item, "computility");
    roofline.computilityName = JsonUtil::GetString(item, "computility_name");
    auto point = JsonUtil::GetVector<std::string>(item, "point");
    constexpr int pointSizeLimit = 2;
    if (point.size() == pointSizeLimit) {
        roofline.point = point;
    }
    roofline.ratio = JsonUtil::GetString(item, "ratio");
    return roofline;
}
} // end of Dic::Module::Source
