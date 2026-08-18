/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
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

#ifndef PROFILER_SERVER_QUERY_KERNEL_MFU_LIST_HANDLER_H
#define PROFILER_SERVER_QUERY_KERNEL_MFU_LIST_HANDLER_H

#include "TimelineRequestHandler.h"

namespace Dic::Module::Timeline {
class QueryKernelMfuListHandler : public TimelineRequestHandler {
  public:
    QueryKernelMfuListHandler() { command = Protocol::REQ_RES_SYSTEM_VIEW_KERNEL_MFU_LIST; }
    ~QueryKernelMfuListHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
};
} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_QUERY_KERNEL_MFU_LIST_HANDLER_H
