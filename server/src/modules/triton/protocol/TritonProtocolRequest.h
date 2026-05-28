// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
//  *
//  * MindStudio is licensed under Mulan PSL v2.
//  * You can use this software according to the terms and conditions of the Mulan PSL v2.
//  * You may obtain a copy of Mulan PSL v2 at:
//  *
//  *          http://license.coscl.org.cn/MulanPSL2
//  *
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//  * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//  * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//  * See the Mulan PSL v2 for more details.
//  * -------------------------------------------------------------------------
//  *
#ifndef PROFILER_SERVER_TRITON_PROTOCOL_REQUEST_H
#define PROFILER_SERVER_TRITON_PROTOCOL_REQUEST_H
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
#include "JsonUtil.h"
namespace Dic::Protocol {
struct TritonMemoryBlocksRequest : public Request {
    TritonMemoryBlocksRequest() : Request(REQ_RES_TRITON_MEMORY_BLOCKS) {}
    std::string scopeType;
    uint64_t startTimestamp{0};
    uint64_t endTimestamp{std::numeric_limits<uint64_t>::max()};
    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        std::unique_ptr<TritonMemoryBlocksRequest> reqPtr = std::make_unique<TritonMemoryBlocksRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        if (!json.HasMember("params")) {
            error = "Request[requestId=" + std::to_string(reqPtr->id) + "] json lacks member params.";
            return nullptr;
        }
        const json_t &param_json = json["params"];
        JsonUtil::SetByJsonKeyValue(reqPtr->startTimestamp, param_json, "_startTimestamp");
        int64_t endVal = -1;
        JsonUtil::SetByJsonKeyValue(endVal, param_json, "_endTimestamp");
        if (endVal < 0) {
            reqPtr->endTimestamp = std::numeric_limits<uint64_t>::max();
        } else {
            reqPtr->endTimestamp = static_cast<uint64_t>(endVal);
        }
        reqPtr->scopeType = JsonUtil::GetString(param_json, "scope");
        return reqPtr;
    }
};

struct TritonBasicInfoRequest : public Request {
    TritonBasicInfoRequest() : Request(REQ_RES_TRITON_MEMORY_BASIC_INFO) {}
    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        std::unique_ptr<TritonBasicInfoRequest> reqPtr = std::make_unique<TritonBasicInfoRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        return reqPtr;
    }
};

struct TritonMemoryUsageRequest : public Request {
    TritonMemoryUsageRequest() : Request(REQ_RES_TRITON_MEMORY_USAGE) {}
    uint64_t timestamp{0};
    std::string scopeType;
    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        std::unique_ptr<TritonMemoryUsageRequest> reqPtr = std::make_unique<TritonMemoryUsageRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        if (!json.HasMember("params")) {
            error = "Request[requestId=" + std::to_string(reqPtr->id) + "] json lacks member params.";
            return nullptr;
        }
        const json_t &param_json = json["params"];
        JsonUtil::SetByJsonKeyValue(reqPtr->timestamp, param_json, "timestamp");
        JsonUtil::SetByJsonKeyValue(reqPtr->scopeType, param_json, "scope");
        return reqPtr;
    }
};
}
#endif
