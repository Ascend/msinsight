// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
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
#ifndef PROFILER_SERVER_TRITON_REQUEST_HANDLER_H
#define PROFILER_SERVER_TRITON_REQUEST_HANDLER_H
#include "ModuleRequestHandler.h"
#include "ProtocolDefs.h"
namespace Dic::Module::Triton {
class TritonRequestHandler : public ModuleRequestHandler {
  public:
    TritonRequestHandler() {
        moduleName = MODULE_TRITON;
        async = false;
    }
    ~TritonRequestHandler() override = default;

  protected:
    inline static const std::string REQUEST_ERROR_UNKNOWN =
        "An unknown exception occurred while querying data. Please check whether your data contains anomalies or "
        "review the logs for more information.";
};
}
#endif
