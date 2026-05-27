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

#ifndef PROFILER_SERVER_SERVER_SRC_MODULES_SUMMARY_HANDLER_QUERYPARALLELISMARRANGEMENTHANDLER_H
#define PROFILER_SERVER_SERVER_SRC_MODULES_SUMMARY_HANDLER_QUERYPARALLELISMARRANGEMENTHANDLER_H

#include "SummaryProtocolResponse.h"
#include "SummaryRequestHandler.h"
namespace Dic::Module::Summary {
class QueryParallelismArrangementHandler : public SummaryRequestHandler {
  public:
    QueryParallelismArrangementHandler() {
        command = Protocol::REQ_RES_PARALLELISM_ARRANGEMENT_ALL;
        async = false;
    }
    ~QueryParallelismArrangementHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    static bool QueryArrangementByDimension(const std::string &projectName, std::string &err,
        const QueryParallelismArrangementRequest &request, ParallelismArrangementResponse &response);
};
}
#endif // PROFILER_SERVER_SERVER_SRC_MODULES_SUMMARY_HANDLER_QUERYPARALLELISMARRANGEMENTHANDLER_H
