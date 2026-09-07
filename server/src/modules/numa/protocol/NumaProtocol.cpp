/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
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
