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

#ifndef PROFILER_SERVER_ADVISORREQUESTHANDLER_H
#define PROFILER_SERVER_ADVISORREQUESTHANDLER_H

#include "ModuleRequestHandler.h"
#include "ProtocolDefs.h"
#include "AdvisorErrorManager.h"

namespace Dic::Module::Advisor {
class AdvisorRequestHandler : public ModuleRequestHandler {
  public:
    AdvisorRequestHandler() {
        moduleName = MODULE_ADVISOR;
        async = false;
    }
    ~AdvisorRequestHandler() override = default;
    bool HandleRequest(std::unique_ptr<Dic::Protocol::Request> requestPtr) override { return true; }
};
} // Advisor

#endif // PROFILER_SERVER_ADVISORREQUESTHANDLER_H
