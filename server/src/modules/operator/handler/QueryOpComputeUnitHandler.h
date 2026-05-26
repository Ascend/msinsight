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

#ifndef PROFILER_SERVER_QUERYOPCOMPUTEUNITHANDLER_H
#define PROFILER_SERVER_QUERYOPCOMPUTEUNITHANDLER_H

#include "ModuleRequestHandler.h"
#include "OperatorRequestHandler.h"
#include "OperatorProtocolRequest.h"

namespace Dic::Module::Operator {
class QueryOpComputeUnitHandler : public OperatorRequestHandler {
  public:
    QueryOpComputeUnitHandler() { command = Protocol::REQ_RES_OPERATOR_COMPUTE_UNIT_INFO; }

    ~QueryOpComputeUnitHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    bool CheckRequestParam(OperatorDurationReqParams params);
};
}

#endif // PROFILER_SERVER_QUERYOPCOMPUTEUNITHANDLER_H
