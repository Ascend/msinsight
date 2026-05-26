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

#ifndef PROFILER_SERVER_MEMORYPROTOCOLREQUEST_H
#define PROFILER_SERVER_MEMORYPROTOCOLREQUEST_H

#include <string>
#include <optional>
#include <MemoryTableColum.h>
#include "CommonRequests.h"
#include "MemoryTableView.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
namespace NSStaticOpTableView = Module::Memory::StaticOpTableView;
const std::string MEMORY_OVERALL_GROUP = "Overall";
const std::string MEMORY_STREAM_GROUP = "Stream";
const std::string MEMORY_COMPONENT_GROUP = "Component";
const std::vector<std::string> operatorTableColumn = {"name", "size", "allocation_time", "release_time", "duration",
    "active_release_time", "active_duration", "allocation_allocated", "allocation_reserve", "allocation_active",
    "release_allocated", "release_reserve", "release_active", "stream", "device_type"};
const std::vector<std::string> componentTableColumn = {"component", "timestamp", "totalReserved", "device"};

struct MemoryOperatorParams : OrderByParam, PaginationParam, FiltersParam, RangeFiltersParam {
    std::string rankId;
    std::string deviceId;
    std::string type;
    std::string searchName;
    int64_t minSize = 0;
    int64_t maxSize = 0;
    double startTime = 0.0;
    double endTime = 0.0;
    bool isCompare = false;
    bool isOnlyShowAllocatedOrReleasedWithinInterval = false;
    bool CommonCheck(std::string &errorMsg, uint64_t minTimeStamp) {
        if (!CheckStrParamValid(rankId, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(deviceId, errorMsg)) {
            return false;
        }
        if (type != MEMORY_OVERALL_GROUP && type != MEMORY_STREAM_GROUP) {
            errorMsg = "Group By parameter should be Overall or Stream.";
            return false;
        }
        if (isCompare && type == MEMORY_STREAM_GROUP) {
            errorMsg = "Memory comparing does not support request type Stream.";
            return false;
        }
        if (startTime > endTime) {
            errorMsg = "Start Time should be smaller or equal to End Time.";
            return false;
        }
        if (endTime > std::numeric_limits<uint64_t>::max() - minTimeStamp) {
            errorMsg = "End Time is invalid.";
            return false;
        }
        return PaginationParam::Check(errorMsg);
    }
};

struct MemoryOperatorSizeParams {
    std::string rankId;
    std::string deviceId;
    std::string type;
    bool isCompare = false;
    bool CommonCheck(std::string &errorMsg) {
        if (!CheckStrParamValid(rankId, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(deviceId, errorMsg)) {
            return false;
        }
        if (type != MEMORY_OVERALL_GROUP && type != MEMORY_STREAM_GROUP) {
            errorMsg = "Group By parameter should be Overall or Stream.";
            return false;
        }
        if (isCompare && type == MEMORY_STREAM_GROUP) {
            errorMsg = "Memory comparing does not support request type Stream.";
            return false;
        }
        return true;
    }
};

struct StaticOperatorListParams : OrderByParam, PaginationParam {
    std::string rankId;
    std::string graphId;
    std::string searchName;
    int64_t minSize = 0;
    int64_t maxSize = 0;
    int64_t startNodeIndex = 0;
    int64_t endNodeIndex = 0;
    bool isCompare = false;
    bool CommonCheck(std::string &errorMsg) {
        if (!CheckStrParamValid(rankId, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(graphId, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(searchName, errorMsg)) {
            return false;
        }
        if (minSize > maxSize) {
            errorMsg = "Min Size should be smaller or equal to Max Size.";
            return false;
        }
        if (startNodeIndex > endNodeIndex) {
            errorMsg = "Start Node Index should be smaller or equal to End Node Index.";
            return false;
        }
        if (!CheckPageValid(pageSize, currentPage, errorMsg)) {
            return false;
        }
        return PaginationParam::Check(errorMsg);
    }
};

struct StaticOperatorGraphParams {
    std::string rankId;
    std::string modelName;
    std::string graphId;
    bool isCompare = false;
    bool CommonCheck(std::string &errorMsg) {
        if (!CheckStrParamValid(rankId, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(modelName, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(graphId, errorMsg)) {
            return false;
        }
        return true;
    }
};

struct StaticOperatorSizeParams {
    std::string rankId;
    std::string graphId;
    bool isCompare = false;
    bool CommonCheck(std::string &errorMsg) {
        if (!CheckStrParamValid(rankId, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(graphId, errorMsg)) {
            return false;
        }
        return true;
    }
};

struct MemoryTypeRequest : public Request {
    MemoryTypeRequest() : Request(REQ_RES_MEMORY_TYPE) {};
    std::string rankId;
};

struct MemoryResourceTypeRequest : public Request {
    MemoryResourceTypeRequest() : Request(REQ_RES_MEMORY_RESOURCE_TYPE) {};
    std::string rankId;
};

struct MemoryOperatorRequest : public Request {
    MemoryOperatorRequest() : Request(REQ_RES_MEMORY_OPERATOR) {};
    MemoryOperatorParams params;
};

struct MemoryOperatorSizeRequest : public Request {
    MemoryOperatorSizeRequest() : Request(REQ_RES_MEMORY_OPERATOR_MIN_MAX) {};
    MemoryOperatorSizeParams params;
};

struct MemoryComponentParams {
    std::string rankId;
    std::string deviceId;
    int64_t currentPage = 0;
    int64_t pageSize = 0;
    std::string orderBy;
    std::string order;
    bool isCompare = false;
    bool CommonCheck(std::string &errorMsg) {
        if (!CheckStrParamValid(rankId, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(deviceId, errorMsg)) {
            return false;
        }
        if (!CheckPageValid(pageSize, currentPage, errorMsg)) {
            return false;
        }
        if (!order.empty() && order != "ascend" && order != "descend") {
            errorMsg =
                "Order parameter is not legal: Order parameter is " + order + ", it should be ascend or descend.";
            return false;
        }
        if (!orderBy.empty() &&
            std::find(componentTableColumn.begin(), componentTableColumn.end(), orderBy) ==
                componentTableColumn.end()) {
            errorMsg = "Order By parameter is not legal: Order By parameter is " + orderBy +
                ", it should be one of the column names";
            return false;
        }
        return true;
    }
};

struct MemoryComponentRequest : public Request {
    MemoryComponentRequest() : Request(REQ_RES_MEMORY_COMPONENT) {};
    MemoryComponentParams params;
};

struct MemoryViewParams {
    std::string rankId;
    std::string deviceId;
    std::string type; // Overall, Stream, Component
    std::string start;
    std::string end;
    bool isCompare = false;
    bool CommonCheck(std::string &errorMsg) {
        if (!CheckStrParamValid(rankId, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(deviceId, errorMsg)) {
            return false;
        }
        if (type != MEMORY_OVERALL_GROUP && type != MEMORY_STREAM_GROUP && type != MEMORY_COMPONENT_GROUP) {
            errorMsg = "Group By parameter should be Overall or Stream or Component.";
            return false;
        }
        if (isCompare && type == MEMORY_STREAM_GROUP) {
            errorMsg = "Memory comparing does not support request type Stream.";
            return false;
        }
        return true;
    }

    std::string GetRequestKey() {
        std::string res = rankId + "@@@@" + type + "@@@@";
        if (isCompare) {
            res += "IsCompare";
        } else {
            res += "NotCompare";
        }
        return res;
    }
};

struct MemoryFindSliceParams {
    std::string rankId;
    std::string id;
    std::string name;
    bool CommonCheck(std::string &errorMsg) {
        if (!CheckStrParamValid(rankId, errorMsg)) {
            errorMsg = "The value of request param [fileId] is invalid, detail:" + errorMsg;
            return false;
        }
        if (!CheckStrParamValid(id, errorMsg)) {
            errorMsg = "The value of request param [id] is invalid, detail:" + errorMsg;
            return false;
        }
        return true;
    }
};

struct MemoryFindSliceRequest : public Request {
    MemoryFindSliceRequest() : Request(REQ_RES_MEMORY_FIND_SLICE) {};
    MemoryFindSliceParams params;
};

struct MemoryViewRequest : public Request {
    MemoryViewRequest() : Request(REQ_RES_MEMORY_VIEW) {};
    MemoryViewParams params;
};

struct MemoryStaticOperatorGraphRequest : public Request {
    MemoryStaticOperatorGraphRequest() : Request(REQ_RES_MEMORY_STATIC_OP_MEMORY_GRAPH) {};
    StaticOperatorGraphParams params;
};

struct MemoryStaticOperatorListRequest : public Request {
    MemoryStaticOperatorListRequest() : Request(REQ_RES_MEMORY_STATIC_OP_MEMORY_LIST) {};
    StaticOperatorListParams params;

    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        std::unique_ptr<MemoryStaticOperatorListRequest> reqPtr = std::make_unique<MemoryStaticOperatorListRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.graphId, json["params"], "graphId");

        if (json["params"].HasMember("startNodeIndex")) {
            reqPtr->params.startNodeIndex = JsonUtil::GetInteger(json["params"], "startNodeIndex");
        } else {
            reqPtr->params.startNodeIndex = -1;
        }
        if (json["params"].HasMember("endNodeIndex")) {
            reqPtr->params.endNodeIndex = JsonUtil::GetInteger(json["params"], "endNodeIndex");
        } else {
            reqPtr->params.endNodeIndex = -1;
        }
        if (!reqPtr->params.SetOrderFromJson(json["params"], NSStaticOpTableView::FIELD_FULL_COLUMNS, error)) {
            Server::ServerLog::Error("Failed set order from json param: %", error);
            return nullptr;
        }
        reqPtr->params.SetPaginationParamFromJson(json["params"]);
        if (json["params"].HasMember("minSize")) {
            JsonUtil::SetByJsonKeyValue(reqPtr->params.minSize, json["params"], "minSize");
        } else {
            reqPtr->params.minSize = std::numeric_limits<int64_t>::min();
        }
        if (json["params"].HasMember("maxSize")) {
            JsonUtil::SetByJsonKeyValue(reqPtr->params.maxSize, json["params"], "maxSize");
        } else {
            reqPtr->params.maxSize = std::numeric_limits<int64_t>::max();
        }
        JsonUtil::SetByJsonKeyValue(reqPtr->params.searchName, json["params"], "searchName");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
        return reqPtr;
    }
};

struct MemoryStaticOperatorSizeRequest : public Request {
    MemoryStaticOperatorSizeRequest() : Request(REQ_RES_MEMORY_STATIC_OP_MEMORY_MIN_MAX) {};
    StaticOperatorSizeParams params;
};

} // end of namespace Protocol
} // end of namespace Dic

#endif // PROFILER_SERVER_MEMORYPROTOCOLREQUEST_H
