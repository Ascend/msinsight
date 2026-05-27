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

#ifndef DIC_SOURCE_PROTOCOL_SOURCE_REQUEST_H
#define DIC_SOURCE_PROTOCOL_SOURCE_REQUEST_H

#include <string>
#include <optional>
#include <vector>
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
struct SourceCodeFileParams {
    std::string sourceName;
    std::tuple<bool, std::string> Valid() {
        std::string errMsg;
        bool res = CheckStrParamValid(sourceName, errMsg);
        return {res, errMsg};
    }
};

struct SourceCodeFileRequest : public Request {
    SourceCodeFileRequest() : Request(REQ_RES_SOURCE_CODE_FILE) {};
    SourceCodeFileParams params;
};

struct SourceApiLineParams {
    std::string coreName;
    std::string sourceName;
    std::tuple<bool, std::string> Valid() {
        std::string errMsg;
        bool res = CheckStrParamValid(coreName, errMsg);
        if (!res) {
            return {res, errMsg};
        }
        res = CheckStrParamValid(sourceName, errMsg);
        return {res, errMsg};
    }
};

struct SourceApiLineRequest : public Request {
    SourceApiLineRequest() : Request(REQ_RES_SOURCE_API_LINE) {};
    SourceApiLineParams params;
};

struct SourceApiLineDynamicRequest : public Request {
    SourceApiLineDynamicRequest() : Request(REQ_RES_SOURCE_API_LINE_DYNAMIC) {};
    SourceApiLineParams params;
};

struct SourceApiInstrParams {
    std::string coreName;
    std::tuple<bool, std::string> Valid() {
        std::string errMsg;
        bool res = CheckStrParamValid(coreName, errMsg);
        if (!res) {
            return {res, errMsg};
        }
        return {res, errMsg};
    }
};

struct SourceApiInstrDynamicRequest : public Request {
    SourceApiInstrDynamicRequest() : Request(REQ_RES_SOURCE_API_INSTRUCTIONS) {};
    SourceApiInstrParams params;
};

struct SourceApiInstrRequest : public Request {
    SourceApiInstrRequest() : Request(REQ_RES_SOURCE_API_INSTRUCTIONS) {};
};

struct SourceDetailBaseInfoParams {
    bool isCompared = false;
};

struct SourceDetailBaseInfoRequest : public Request {
    SourceDetailBaseInfoRequest() : Request(REQ_RES_DETAILS_BASE_INFO) {};
    SourceDetailBaseInfoParams params;
};

struct SourceDetailLoadInfoParams {
    bool isCompared = false;
};

struct SourceDetailsLoadInfoRequest : public Request {
    SourceDetailsLoadInfoRequest() : Request(REQ_RES_DETAILS_COMPUTE_LOAD_INFO) {};
    SourceDetailLoadInfoParams params;
};

struct DetailsMemoryInfoParams {
    std::string blockId;
    bool isCompared = false;
    std::tuple<bool, std::string> Valid() {
        std::string errMsg;
        bool res = CheckStrParamValid(blockId, errMsg);
        return {res, errMsg};
    }
};

struct DetailsMemoryGraphRequest : public Request {
    DetailsMemoryGraphRequest() : Request(REQ_RES_DETAILS_COMPUTE_MEMORY_GRAPH) {};
    DetailsMemoryInfoParams params;
};

struct DetailsMemoryTableRequest : public Request {
    DetailsMemoryTableRequest() : Request(REQ_RES_DETAILS_COMPUTE_MEMORY_TABLE) {};
    DetailsMemoryInfoParams params;
};

struct DetailsInterCoreLoadGraphParams {
    bool isCompared = false;
};

struct DetailsInterCoreLoadGraphRequest : public Request {
    DetailsInterCoreLoadGraphRequest() : Request(REQ_RES_DETAILS_INTER_CORE_LOAD_GRAPH) {};
    DetailsInterCoreLoadGraphParams params;
};

struct DetailsRooflineParams {};

struct DetailsRooflineRequest : public Request {
    DetailsRooflineRequest() : Request(REQ_RES_DETAILS_ROOFLINE) {};
    DetailsRooflineParams params;
};

} // end of namespace Protocol
} // end of namespace Dic

#endif // DIC_SOURCE_PROTOCOL_SOURCE_REQUEST_H
