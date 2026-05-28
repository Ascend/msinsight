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
#ifndef PROFILER_SERVER_TRITONMODULE_H
#define PROFILER_SERVER_TRITONMODULE_H
#include "BaseModule.h"
#include "ProtocolDefs.h"
namespace Dic {
namespace Module::Triton {
class TritonModule : public BaseModule {
  public:
    TritonModule() { moduleName = MODULE_TRITON; }
    ~TritonModule() override = default;
    void RegisterRequestHandlers() override;
    void OnRequest(std::unique_ptr<Protocol::Request> request) override;
};
}
} // Dic

#endif //PROFILER_SERVER_TRITONMODULE_H
