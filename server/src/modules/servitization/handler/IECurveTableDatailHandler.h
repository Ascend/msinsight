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

#ifndef PROFILER_SERVER_IECURVETABLEDATAILHANDLER_H
#define PROFILER_SERVER_IECURVETABLEDATAILHANDLER_H
#include "BaseDomain.h"
#include "CurveRepo.h"
#include "IEProtocolResponse.h"
#include "IERequestHandler.h"
namespace Dic::Module::IE {
class IECurveTableDatailHandler : public IERequestHandler {
  public:
    IECurveTableDatailHandler() { moduleName = MODULE_IE; }
    ~IECurveTableDatailHandler() override = default;
    bool HandleRequest(std::unique_ptr<Dic::Protocol::Request> requestPtr) override;

  protected:
    std::shared_ptr<CurveRepo> repo = std::make_shared<CurveRepo>();
    void QueryViewData(IETableViewResponse &response, const PageQuery &query);
};
}
#endif // PROFILER_SERVER_IECURVETABLEDATAILHANDLER_H
