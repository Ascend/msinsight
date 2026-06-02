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

#ifndef PROFILER_SERVER_IEPROTOCOLRESQUEST_H
#define PROFILER_SERVER_IEPROTOCOLRESQUEST_H
#include <string>
#include <optional>
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
namespace Dic::Protocol {
struct IEUsageViewParams {
    std::string rankId;
    std::string type;
    bool isZh = true;
};

struct IEUsageViewParamsRequest : public Request {
    IEUsageViewParamsRequest() : Request(REQ_RES_IE_VIEW) {};
    IEUsageViewParams params;
};

struct IETableViewParams {
    std::string rankId;
    std::string type;
    uint64_t pageSize = 0;
    uint64_t currentPage = 0;
    std::string startTime;
    std::string endTime;
    std::string order;
    std::string orderBy;
    bool CommonCheck(std::string &errorMsg) const {
        static const uint64_t PAGE_LIMIT = 500;
        if (pageSize == 0 || pageSize > PAGE_LIMIT) {
            errorMsg = "Page size invalid!";
            return false;
        }
        if (currentPage == 0 || currentPage - 1 > UINT64_MAX / pageSize) {
            errorMsg = "Current page invalid!";
            return false;
        }
        return true;
    }
};

struct IETableRequest : public Request {
    IETableRequest() : Request(REQ_RES_IE_TABLE_VIEW) {};
    IETableViewParams params;
};

struct IEGroupParams {
    std::string rankId;
};

struct IEGroupRequest : public Request {
    IEGroupRequest() : Request(REQ_RES_IE_DATA_GROUP) {};
    IEGroupParams params;
};
} // namespace Dic::Protocol

#endif // PROFILER_SERVER_IEPROTOCOLRESQUEST_H
