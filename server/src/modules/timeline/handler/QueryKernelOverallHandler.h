/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_QUERYKERNELOVERALLHANDLER_H
#define PROFILER_SERVER_QUERYKERNELOVERALLHANDLER_H

#include <memory>
#include <string>

#include "KernelDetailOverallDatabaseAccesser.h"
#include "TimelineRequestHandler.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Timeline {

void BuildKernelOverallResult(const std::vector<KernelDetailOverallRecord> &records, KernelOverallResponse &response,
    uint32_t current, uint32_t pageSize);

class QueryKernelOverallHandler : public TimelineRequestHandler {
  public:
    QueryKernelOverallHandler() { command = Protocol::REQ_RES_KERNEL_OVERALL; };

    ~QueryKernelOverallHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    static bool CalKernelData(KernelOverallRequest &request, KernelOverallResponse &response, std::string &error,
        const std::shared_ptr<VirtualTraceDatabase> &database);
};
} // end of namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_QUERYKERNELOVERALLHANDLER_H
