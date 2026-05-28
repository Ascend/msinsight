// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026$ Huawei Technologies Co.,Ltd.
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

//
// Created by 且依且形 on 2026/2/10.
//

#include "TritonProtocol.h"
#include "TritonProtocolEvent.h"
#include "TritonProtocolRequest.h"
#include "TritonProtocolResponse.h"

namespace Dic::Protocol {
void TritonProtocolUtil::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(
        REQ_RES_TRITON_MEMORY_BLOCKS, ProtocolUtil::BuildRequestFromJson<TritonMemoryBlocksRequest>);
    jsonToReqFactory.emplace(
        REQ_RES_TRITON_MEMORY_BASIC_INFO, ProtocolUtil::BuildRequestFromJson<TritonBasicInfoRequest>);
    jsonToReqFactory.emplace(REQ_RES_TRITON_MEMORY_USAGE, ProtocolUtil::BuildRequestFromJson<TritonMemoryUsageRequest>);
}
void TritonProtocolUtil::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_TRITON_MEMORY_BLOCKS, ProtocolUtil::CommonResponseToJson);
    resToJsonFactory.emplace(REQ_RES_TRITON_MEMORY_BASIC_INFO, ProtocolUtil::CommonResponseToJson);
    resToJsonFactory.emplace(REQ_RES_TRITON_MEMORY_USAGE, ProtocolUtil::CommonResponseToJson);
}
void TritonProtocolUtil::RegisterEventToJsonFuncs() {
    eventToJsonFactory.emplace(std::string(EVENT_PARSE_TRITON_COMPLETED), ProtocolUtil::CommonEventToJson);
}
} // namespace Dic::Protocol
// Dic
