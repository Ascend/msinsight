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

#include "NumaProtocol.h"

namespace Dic::Protocol {
void NumaProtocolUtil::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(REQ_RES_NUMA_OVERVIEW, ProtocolUtil::BuildRequestFromJson<NumaOverviewRequest>);
}

void NumaProtocolUtil::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_NUMA_OVERVIEW, ProtocolUtil::CommonResponseToJson);
}

void NumaProtocolUtil::RegisterEventToJsonFuncs() {}
} // namespace Dic::Protocol
