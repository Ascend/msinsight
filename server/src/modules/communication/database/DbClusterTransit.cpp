/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "DbClusterDataBase.h"
#include "ClusterStatisticsQuery.h"
#include "JsonUtil.h"
#include <set>

namespace Dic::Module::FullDb {
namespace {
bool IsPacketMetric(double value) { return std::isfinite(value) && value >= 0; }

bool ConvertPacketDistribution(const std::string &source, double transitTime, std::string &distribution) {
    document_t packets;
    packets.Parse(source.c_str(), source.size());
    if (packets.HasParseError() || !packets.IsArray()) {
        return false;
    }

    document_t output(rapidjson::kObjectType);
    auto &allocator = output.GetAllocator();
    std::set<double> sizes;
    for (const auto &packet : packets.GetArray()) {
        if (!packet.IsObject() || !packet.HasMember("package_size") || !packet["package_size"].IsNumber() ||
            !packet.HasMember("count") || !packet["count"].IsNumber()) {
            return false;
        }
        const auto &packetSize = packet["package_size"];
        const double size = packetSize.GetDouble();
        const double count = packet["count"].GetDouble();
        if (!IsPacketMetric(size) || !IsPacketMetric(count) || !sizes.insert(size).second) {
            return false;
        }
        const std::string key = JsonUtil::JsonDump(packetSize);
        json_t values(rapidjson::kArrayType);
        values.PushBack(count, allocator);
        values.PushBack(transitTime, allocator);
        output.AddMember(json_t(key.c_str(), allocator), values, allocator);
    }
    distribution = JsonUtil::JsonDump(output);
    return true;
}
}

bool DbClusterDataBase::QueryTransitBandwidth(
    const Protocol::BandwidthDataParam &param, Protocol::BandwidthDataResBody &resBody) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const std::string step = param.iterationId.rfind("step", 0) == 0 ? param.iterationId : "step" + param.iterationId;
    auto statement = PrepareStatisticsQuery(*this,
        "SELECT band_type, transit_size, transit_time, bandwidth FROM TransitStatistic "
        "WHERE step = ? AND rank_id = ? AND group_name = ? ORDER BY band_type COLLATE BINARY",
        {step, param.rankId, param.groupIdHash});
    if (statement == nullptr) {
        return false;
    }

    std::vector<Protocol::BandwidthDataItem> items;
    std::set<std::string> types;
    auto *row = statement->stmt;
    int status;
    while ((status = sqlite3_step(row)) == SQLITE_ROW) {
        Protocol::BandwidthDataItem item;
        if (!ReadStatisticsText(row, 0, item.transportType) || !ReadStatisticsMetric(row, 1, item.transitSize) ||
            !ReadStatisticsMetric(row, 2, item.transitTime) || !ReadStatisticsMetric(row, 3, item.bandwidth) ||
            !types.insert(item.transportType).second) {
            return false;
        }
        items.push_back(std::move(item));
    }
    if (status != SQLITE_DONE) {
        return false;
    }
    resBody.items = std::move(items);
    return true;
}

bool DbClusterDataBase::QueryTransitDistribution(
    const Protocol::DistributionDataParam &param, Protocol::DistributionResBody &resBody) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const std::string step = param.iterationId.rfind("step", 0) == 0 ? param.iterationId : "step" + param.iterationId;
    auto statement = PrepareStatisticsQuery(*this,
        "SELECT package_distribution, transit_time FROM TransitStatistic "
        "WHERE step = ? AND rank_id = ? AND group_name = ? AND band_type = ?",
        {step, param.rankId, param.groupIdHash, param.transportType});
    if (statement == nullptr) {
        return false;
    }

    auto *row = statement->stmt;
    const int status = sqlite3_step(row);
    if (status == SQLITE_DONE) {
        resBody.distributionData.clear();
        return true;
    }
    std::string source;
    double transitTime = 0;
    std::string distribution;
    if (status != SQLITE_ROW || !ReadStatisticsText(row, 0, source) || !ReadStatisticsMetric(row, 1, transitTime) ||
        !ConvertPacketDistribution(source, transitTime, distribution) || sqlite3_step(row) != SQLITE_DONE) {
        return false;
    }
    resBody.distributionData = std::move(distribution);
    return true;
}
}
