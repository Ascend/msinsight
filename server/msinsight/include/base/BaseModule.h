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
#ifndef MSINSIGHT_BASE_MODULE_H
#define MSINSIGHT_BASE_MODULE_H
#include "ProtocolUtil.h"
#include "ModuleRequestHandler.h"
namespace Dic::Module {
class BaseModule {
  public:
    BaseModule() = default;
    virtual ~BaseModule() = default;
    virtual void RegisterRequestHandlers() = 0;
    virtual void OnRequest(std::unique_ptr<Protocol::Request> request);

  protected:
    std::string moduleName = MODULE_UNKNOWN;
    std::map<std::string, std::unique_ptr<ModuleRequestHandler>> requestHandlerMap;
};
} // end of namespace Module
#endif // MSINSIGHT_BASE_MODULE_H
