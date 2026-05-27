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

#ifndef PROFILER_SERVER_QUERYDETAILSLOADINFOHANDLER_H
#define PROFILER_SERVER_QUERYDETAILSLOADINFOHANDLER_H

#include "SourceRequestHandler.h"

namespace Dic {
namespace Module {
namespace Source {
class QueryDetailsLoadInfoHandler : public SourceRequestHandler {
  public:
    QueryDetailsLoadInfoHandler() { command = Protocol::REQ_RES_DETAILS_COMPUTE_LOAD_INFO; }
    ~QueryDetailsLoadInfoHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
};
}
}
}
#endif // PROFILER_SERVER_QUERYDETAILSLOADINFOHANDLER_H
