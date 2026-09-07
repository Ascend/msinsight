/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_NUMA_PROTOCOL_H
#define PROFILER_SERVER_NUMA_PROTOCOL_H

#include <limits>
#include <string>
#include <vector>

#include "JsonUtil.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"

namespace Dic::Protocol {

class NumaProtocolUtil : public ProtocolUtil {
  public:
    NumaProtocolUtil() noexcept = default;
    ~NumaProtocolUtil() noexcept override = default;
    void RegisterJsonToRequestFuncs() override;
    void RegisterResponseToJsonFuncs() override;
    void RegisterEventToJsonFuncs() override;
};

struct NumaOverviewRequest : public Request {
    NumaOverviewRequest() : Request(REQ_RES_NUMA_OVERVIEW) {}

    std::string rankId;
    uint64_t startTime = 0;
    uint64_t endTime = 0;

    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        auto request = std::make_unique<NumaOverviewRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*request, json)) {
            error = "Failed to set NUMA overview request base info";
            return nullptr;
        }
        if (!json.HasMember("params")) {
            error = "NUMA overview request lacks params";
            return nullptr;
        }
        const json_t &params = json["params"];
        JsonUtil::SetByJsonKeyValue(request->rankId, params, "rankId");
        JsonUtil::SetByJsonKeyValue(request->startTime, params, "startTime");
        JsonUtil::SetByJsonKeyValue(request->endTime, params, "endTime");
        return request;
    }
};

struct NumaMetricData {
    std::string key;
    std::string label;
    std::string description;
    std::string unit;
    double value = 0;
    bool hasValue = true;
};

struct NumaNodeData {
    int id = 0;
    std::string name;
    std::vector<NumaMetricData> metrics;
};

struct NumaSocketData {
    int id = 0; // Socket ID, 0-based
    std::string name; // Socket 名称
    std::vector<NumaMetricData> metrics; // Socket 汇总指标
    std::vector<NumaNodeData> numas; // Socket 下的 NUMA 节点
};

struct NumaConnectionData {
    std::string id;
    std::string type;
    std::string source;
    std::string target;
    std::string label;
    std::string description;
    std::vector<NumaMetricData> metrics;
};

struct NumaOverviewResponse : public JsonResponse {
    NumaOverviewResponse() : JsonResponse(REQ_RES_NUMA_OVERVIEW) {}

    uint64_t rangeStart = 0;
    uint64_t rangeEnd = 0;
    std::vector<NumaMetricData> totalMetrics;
    std::vector<NumaSocketData> sockets;
    std::vector<NumaConnectionData> connections;

    static json_t MetricToJson(const NumaMetricData &metric, document_t::AllocatorType &allocator) {
        json_t json(kObjectType);
        JsonUtil::AddMember(json, "key", metric.key, allocator);
        JsonUtil::AddMember(json, "label", metric.label, allocator);
        JsonUtil::AddMember(json, "description", metric.description, allocator);
        JsonUtil::AddMember(json, "unit", metric.unit, allocator);
        JsonUtil::AddMember(json, "value", metric.value, allocator);
        JsonUtil::AddMember(json, "hasValue", metric.hasValue, allocator);
        return json;
    }

    static json_t MetricsToJson(const std::vector<NumaMetricData> &metrics, document_t::AllocatorType &allocator) {
        json_t json(kArrayType);
        for (const auto &metric : metrics) {
            json.PushBack(MetricToJson(metric, allocator), allocator);
        }
        return json;
    }

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);

        json_t body(kObjectType);
        json_t range(kObjectType);
        JsonUtil::AddMember(range, "startTime", rangeStart, allocator);
        JsonUtil::AddMember(range, "endTime", rangeEnd, allocator);
        JsonUtil::AddMember(body, "range", range, allocator);
        JsonUtil::AddMember(body, "totalMetrics", MetricsToJson(totalMetrics, allocator), allocator);

        json_t socketArray(kArrayType);
        for (const auto &socket : sockets) {
            json_t socketJson(kObjectType);
            JsonUtil::AddMember(socketJson, "id", socket.id, allocator);
            JsonUtil::AddMember(socketJson, "name", socket.name, allocator);
            JsonUtil::AddMember(socketJson, "metrics", MetricsToJson(socket.metrics, allocator), allocator);
            json_t numaArray(kArrayType);
            for (const auto &numa : socket.numas) {
                json_t numaJson(kObjectType);
                JsonUtil::AddMember(numaJson, "id", numa.id, allocator);
                JsonUtil::AddMember(numaJson, "name", numa.name, allocator);
                JsonUtil::AddMember(numaJson, "metrics", MetricsToJson(numa.metrics, allocator), allocator);
                numaArray.PushBack(numaJson, allocator);
            }
            JsonUtil::AddMember(socketJson, "numas", numaArray, allocator);
            socketArray.PushBack(socketJson, allocator);
        }
        JsonUtil::AddMember(body, "sockets", socketArray, allocator);

        json_t connectionArray(kArrayType);
        for (const auto &connection : connections) {
            json_t connectionJson(kObjectType);
            JsonUtil::AddMember(connectionJson, "id", connection.id, allocator);
            JsonUtil::AddMember(connectionJson, "type", connection.type, allocator);
            JsonUtil::AddMember(connectionJson, "source", connection.source, allocator);
            JsonUtil::AddMember(connectionJson, "target", connection.target, allocator);
            JsonUtil::AddMember(connectionJson, "label", connection.label, allocator);
            JsonUtil::AddMember(connectionJson, "description", connection.description, allocator);
            JsonUtil::AddMember(connectionJson, "metrics", MetricsToJson(connection.metrics, allocator), allocator);
            connectionArray.PushBack(connectionJson, allocator);
        }
        JsonUtil::AddMember(body, "connections", connectionArray, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

} // namespace Dic::Protocol

#endif // PROFILER_SERVER_NUMA_PROTOCOL_H
