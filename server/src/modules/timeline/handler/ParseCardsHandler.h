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

#ifndef PROFILER_SERVER_PARSE_CARDS_HANDLER_H
#define PROFILER_SERVER_PARSE_CARDS_HANDLER_H

#include <set>
#include <regex>
#include "TimelineRequestHandler.h"
#include "ProjectParserFactory.h"
#include "FileParser.h"

namespace Dic {
namespace Module {
namespace Timeline {
class ParseCardsHandler : public TimelineRequestHandler {
  public:
    ParseCardsHandler() {
        command = Protocol::REQ_RES_PARSE_CARDS;
        async = false;
    };
    ~ParseCardsHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_PARSE_CARDS_HANDLER_H
