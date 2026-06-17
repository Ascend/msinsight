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

#ifndef PROFILER_SERVER_SOURCE_PROTOCOL_H
#define PROFILER_SERVER_SOURCE_PROTOCOL_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
class SourceProtocol : public ProtocolUtil {
public:
    SourceProtocol() = default;
    ~SourceProtocol() override = default;

private:
    void RegisterJsonToRequestFuncs() override;
    void RegisterResponseToJsonFuncs() override;
    void RegisterEventToJsonFuncs() override;

    // json to request
    static std::unique_ptr<Request> ToNoParamsRequest(const json_t &json, std::string &error,
                                                      const std::string &command);
    static std::unique_ptr<Request> ToCodeFileRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToApiLineRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToApiLineDynamicRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToApiInstrRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToApiInstrDynamicRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToDetailsBaseInfoRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToDetailsLoadInfoRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToDetailsMemoryGraphRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToDetailsMemoryTableRequest(const Dic::json_t &json, std::string &error);
    static std::unique_ptr<Request> ToDetailsInterCoreLoadGraphRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToDetailsRooflineRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToCachelineRecordRequest(const Dic::json_t &json, std::string &error);
    static std::unique_ptr<Request> ToTopWarpStallReasonRequest(const json_t &json, std::string &error);

    // response to json
    static std::optional<document_t> ToCodeFileResponse(const Response &response);
    static std::optional<document_t> ToApiLineResponse(const Response &response);
    static std::optional<document_t> ToApiLineDynamicResponse(const Response &response);
    static std::optional<document_t> ToApiInstrResponse(const Response &response);
    static std::optional<document_t> ToApiInstrDynamicResponse(const Response &response);
    static std::optional<document_t> ToDetailsBaseInfoResponse(const Response &response);
    static std::optional<document_t> ToDetailsLoadInfoResponse(const Response &response);
    static std::optional<document_t> ToDetailsMemoryGraphResponse(const Response &response);
    static std::optional<document_t> ToDetailsMemoryTableResponse(const Response &response);
    static std::optional<document_t> ToDetailsInterCoreLoadGraphResponse(const Response &response);
    static std::optional<document_t> ToDetailsRooflineResponse(const Response &response);
    static std::optional<document_t> ToCachelineRecordResponse(const Dic::Protocol::Response &response);
    static std::optional<document_t> ToTopWarpStallReasonResponse(const Response &response);
};

} // Protocol
namespace Module::Source {
enum class DataTypeEnum : int {
    SOURCE = 1,
    TRACE = 2,
    API_FILE = 3,
    API_INSTR = 4,
    DETAILS_BASE_INFO = 5,
    DETAILS_COMPUTE_LOAD_GRAPH = 6,
    DETAILS_COMPUTE_LOAD_TABLE = 7,
    DETAILS_MEMORY_GRAPH = 8,
    DETAILS_MEMORY_TABLE = 9,
    DETAILS_INTER_CORE_LOAD_GRAPH = 12,
    DETAILS_ROOFLINE = 13,
    DISPLAY_CACHE = 0x0E,
    TOP_WARP_STALL_REASON = 0x0F,
    OTHER = 0x100
};

struct Position {
    int64_t startPos;
    int64_t endPos;
};
} // Module::Source
} // Dic
#endif // PROFILER_SERVER_SOURCE_PROTOCOL_H
