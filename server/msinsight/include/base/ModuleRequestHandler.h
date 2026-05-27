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
#ifndef MSINSIGHT_REQUEST_HANDLER_H
#define MSINSIGHT_REQUEST_HANDLER_H
#include "ProtocolUtil.h"
#include "WsSender.h"
namespace Dic::Module {
using namespace Dic::Protocol;
class ModuleRequestHandler {
  public:
    ModuleRequestHandler() = default;
    virtual ~ModuleRequestHandler() = default;
    bool HandleRequestEntrance(std::unique_ptr<Request> requestPtr);
    virtual const std::string GetError();
    virtual bool HandleRequest(std::unique_ptr<Request> requestPtr) = 0;
    virtual bool IsAsync();
    virtual std::string GetRequestKey(Request &requestPtr);
    static void SetBaseResponse(const Request &request, Response &response);
    static void SetResponseResult(
        Response &response, bool result, const std::string &errorMsg = "", const int errorCode = UNKNOW_ERROR);
    static void SetResponseErrorFromRequestContext(Response &response);
    static void SetRequestContextError(ErrorMessage error);
    static void ResetRequestContextError();

  protected:
    std::string command;
    std::string error;
    std::string moduleName = MODULE_UNKNOWN;
    bool async = true;
};
} // end of namespace Module
#endif // MSINSIGHT_REQUEST_HANDLER_H
