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

#ifndef PROFILER_SERVER_IEPROTOCOLRESPONSE_H
#define PROFILER_SERVER_IEPROTOCOLRESPONSE_H
#include <map>
#include <string>
#include <vector>
#include <map>
#include <string>
#include "GlobalDefs.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
namespace Dic::Protocol {
struct IEUsageViewData {
    std::string title;
    std::string desc;
    std::vector<std::string> legends;
    std::vector<std::vector<std::string>> lines;
};
struct IEUsageViewResponse : public Response {
    IEUsageViewResponse() : Response(REQ_RES_IE_VIEW) {}
    IEUsageViewData data;
};

struct Column {
    std::string name;
    std::string type;
    std::string key;
};

struct IETableViewData {
    std::vector<Column> columnAttr;
    std::vector<std::map<std::string, std::string>> columnData;
    uint64_t totalNum = 0;
};
struct IETableViewResponse : public Response {
    IETableViewResponse() : Response(REQ_RES_IE_TABLE_VIEW) {}
    IETableViewData data;
};

struct IEGroupData {
    std::string label;
    std::string value;
};
struct IEGroupResponse : public Response {
    IEGroupResponse() : Response(REQ_RES_IE_DATA_GROUP) {}
    std::vector<IEGroupData> data;
};
}

#endif // PROFILER_SERVER_IEPROTOCOLRESPONSE_H
