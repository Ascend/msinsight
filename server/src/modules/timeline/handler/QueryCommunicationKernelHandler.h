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

#ifndef PROFILER_SERVER_COMMUNICATIONKERNERHANDLER_H
#define PROFILER_SERVER_COMMUNICATIONKERNERHANDLER_H

#include "TimelineRequestHandler.h"
#include "VirtualTraceDatabase.h"

namespace Dic {
namespace Module {
namespace Timeline {
class QueryCommunicationKernelHandler : public TimelineRequestHandler {
  public:
    QueryCommunicationKernelHandler() { command = Protocol::REQ_RES_COMMUNICATION_KERNEL_DETAIL; };

    ~QueryCommunicationKernelHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
    std::string GetRealRankId(const std::string &rankId);
    std::shared_ptr<VirtualTraceDatabase> GetTraceDatabaseByRankId(CommunicationKernelRequest &request);
};
}
}
}
#endif // PROFILER_SERVER_COMMUNICATIONKERNERHANDLER_H
