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
#include "SourceProtocolUtil.h"
#include "SourceProtocolRequest.h"
#include "SourceProtocolResponse.h"
#include "SourceProtocol.h"

namespace Dic {
namespace Protocol {
void SourceProtocol::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(REQ_RES_SOURCE_CODE_FILE, ToCodeFileRequest);
    jsonToReqFactory.emplace(REQ_RES_SOURCE_API_LINE, ToApiLineRequest);
    jsonToReqFactory.emplace(REQ_RES_SOURCE_API_LINE_DYNAMIC, ToApiLineDynamicRequest);
    jsonToReqFactory.emplace(REQ_RES_SOURCE_API_INSTRUCTIONS, ToApiInstrRequest);
    jsonToReqFactory.emplace(REQ_RES_SOURCE_API_INSTRUCTIONS_DYNAMIC, ToApiInstrDynamicRequest);
    jsonToReqFactory.emplace(REQ_RES_DETAILS_BASE_INFO, ToDetailsBaseInfoRequest);
    jsonToReqFactory.emplace(REQ_RES_DETAILS_COMPUTE_LOAD_INFO, ToDetailsLoadInfoRequest);
    jsonToReqFactory.emplace(REQ_RES_DETAILS_COMPUTE_MEMORY_GRAPH, ToDetailsMemoryGraphRequest);
    jsonToReqFactory.emplace(REQ_RES_DETAILS_COMPUTE_MEMORY_TABLE, ToDetailsMemoryTableRequest);
    jsonToReqFactory.emplace(REQ_RES_DETAILS_INTER_CORE_LOAD_GRAPH, ToDetailsInterCoreLoadGraphRequest);
    jsonToReqFactory.emplace(std::string(REQ_RES_DETAILS_ROOFLINE), ToDetailsRooflineRequest);
    jsonToReqFactory.emplace(std::string(REQ_RES_CACHELINE_RECORD), ToCachelineRecordRequest);
}

void SourceProtocol::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_SOURCE_CODE_FILE, ToCodeFileResponse);
    resToJsonFactory.emplace(REQ_RES_SOURCE_API_LINE, ToApiLineResponse);
    resToJsonFactory.emplace(REQ_RES_SOURCE_API_LINE_DYNAMIC, ToApiLineDynamicResponse);
    resToJsonFactory.emplace(REQ_RES_SOURCE_API_INSTRUCTIONS, ToApiInstrResponse);
    resToJsonFactory.emplace(REQ_RES_SOURCE_API_INSTRUCTIONS_DYNAMIC, ToApiInstrDynamicResponse);
    resToJsonFactory.emplace(REQ_RES_DETAILS_BASE_INFO, ToDetailsBaseInfoResponse);
    resToJsonFactory.emplace(REQ_RES_DETAILS_COMPUTE_LOAD_INFO, ToDetailsLoadInfoResponse);
    resToJsonFactory.emplace(REQ_RES_DETAILS_COMPUTE_MEMORY_GRAPH, ToDetailsMemoryGraphResponse);
    resToJsonFactory.emplace(REQ_RES_DETAILS_COMPUTE_MEMORY_TABLE, ToDetailsMemoryTableResponse);
    resToJsonFactory.emplace(REQ_RES_DETAILS_INTER_CORE_LOAD_GRAPH, ToDetailsInterCoreLoadGraphResponse);
    resToJsonFactory.emplace(std::string(REQ_RES_DETAILS_ROOFLINE), ToDetailsRooflineResponse);
    resToJsonFactory.emplace(std::string(REQ_RES_CACHELINE_RECORD), ToCachelineRecordResponse);
}

void SourceProtocol::RegisterEventToJsonFuncs() {}

#pragma region <<Json To Request>>

std::unique_ptr<Request> SourceProtocol::ToNoParamsRequest(
    const Dic::json_t &json, std::string &error, const std::string &command) {
    std::unique_ptr<Request> reqPtr = std::make_unique<Request>(command);
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToCodeFileRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SourceCodeFileRequest> reqPtr = std::make_unique<SourceCodeFileRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.sourceName, json["params"], "sourceName");
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToApiLineRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SourceApiLineRequest> reqPtr = std::make_unique<SourceApiLineRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.coreName, json["params"], "coreName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.sourceName, json["params"], "sourceName");
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToApiLineDynamicRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SourceApiLineDynamicRequest> reqPtr = std::make_unique<SourceApiLineDynamicRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.coreName, json["params"], "coreName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.sourceName, json["params"], "sourceName");
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToApiInstrRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SourceApiInstrRequest> reqPtr = std::make_unique<SourceApiInstrRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToApiInstrDynamicRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SourceApiInstrDynamicRequest> reqPtr = std::make_unique<SourceApiInstrDynamicRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.coreName, json["params"], "coreName");
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToDetailsBaseInfoRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SourceDetailBaseInfoRequest> reqPtr = std::make_unique<SourceDetailBaseInfoRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompared, json["params"], "isCompared");
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToDetailsLoadInfoRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SourceDetailsLoadInfoRequest> reqPtr = std::make_unique<SourceDetailsLoadInfoRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompared, json["params"], "isCompared");
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToDetailsMemoryGraphRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<DetailsMemoryGraphRequest> reqPtr = std::make_unique<DetailsMemoryGraphRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.blockId, json["params"], "blockId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompared, json["params"], "isCompared");
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToDetailsMemoryTableRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<DetailsMemoryTableRequest> reqPtr = std::make_unique<DetailsMemoryTableRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.blockId, json["params"], "blockId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompared, json["params"], "isCompared");
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToDetailsInterCoreLoadGraphRequest(
    const Dic::json_t &json, std::string &error) {
    std::unique_ptr<DetailsInterCoreLoadGraphRequest> reqPtr = std::make_unique<DetailsInterCoreLoadGraphRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompared, json["params"], "isCompared");
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToDetailsRooflineRequest(const Dic::json_t &json, std::string &error) {
    auto reqPtr = std::make_unique<DetailsRooflineRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    return reqPtr;
}

std::unique_ptr<Request> SourceProtocol::ToCachelineRecordRequest(const Dic::json_t &json, std::string &error) {
    return ToNoParamsRequest(json, error, std::string(REQ_RES_CACHELINE_RECORD));
}

#pragma endregion

#pragma region <<Reponse To Json>>

std::optional<document_t> SourceProtocol::ToCodeFileResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<SourceCodeFileResponse>(dynamic_cast<const SourceCodeFileResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToApiLineResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<SourceApiLineResponse>(dynamic_cast<const SourceApiLineResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToApiLineDynamicResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<SourceApiLineDynamicResponse>(dynamic_cast<const SourceApiLineDynamicResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToApiInstrResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<SourceApiInstrResponse>(dynamic_cast<const SourceApiInstrResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToApiInstrDynamicResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<SourceApiInstrDynamicResponse>(dynamic_cast<const SourceApiInstrDynamicResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToDetailsBaseInfoResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<DetailsBaseInfoResponse>(dynamic_cast<const DetailsBaseInfoResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToDetailsLoadInfoResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<DetailsLoadInfoResponse>(dynamic_cast<const DetailsLoadInfoResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToDetailsMemoryGraphResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<DetailsMemoryGraphResponse>(dynamic_cast<const DetailsMemoryGraphResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToDetailsMemoryTableResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<DetailsMemoryTableResponse>(dynamic_cast<const DetailsMemoryTableResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToDetailsInterCoreLoadGraphResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<DetailsInterCoreLoadGraphResponse>(
        dynamic_cast<const DetailsInterCoreLoadGraphResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToDetailsRooflineResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<DetailsRooflineResponse>(dynamic_cast<const DetailsRooflineResponse &>(response));
}

std::optional<document_t> SourceProtocol::ToCachelineRecordResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<CachelineRecordResponse>(dynamic_cast<const CachelineRecordResponse &>(response));
}

#pragma endregion

} // namespace Protocol
} // namespace Dic
