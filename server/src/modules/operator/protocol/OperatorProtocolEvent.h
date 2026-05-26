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

#ifndef PROFILER_SERVER_OPERATORPROTOCOLEVENT_H
#define PROFILER_SERVER_OPERATORPROTOCOLEVENT_H

#include <vector>
#include "ProtocolDefs.h"
#include "GlobalDefs.h"
#include "ProtocolMessage.h"

namespace Dic::Protocol {
struct OperatorRank {
    std::string rankId;
    bool status{false};
    std::string error;
};

struct OperatorParseStatusEvent : public Event {
    OperatorParseStatusEvent() : Event(EVENT_PARSE_OPERATOR_STATUS) {}
    OperatorRank data;
    std::string fileId;
    std::vector<RankInfo> rankList;
};

struct OperatorParseClearEvent : public Event {
    OperatorParseClearEvent() : Event(EVENT_PARSE_OPERATOR_CLEAR) {}
};
}

#endif // PROFILER_SERVER_OPERATORPROTOCOLEVENT_H
