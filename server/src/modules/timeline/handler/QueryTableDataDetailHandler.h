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

#ifndef PROFILER_SERVER_QUERYTABLEDATADETAILHANDLER_H
#define PROFILER_SERVER_QUERYTABLEDATADETAILHANDLER_H
#include "VirtualTraceDatabase.h"
#include "TimelineRequestHandler.h"
namespace Dic::Module::Timeline {
class QueryTableDataDetailHandler : public TimelineRequestHandler {
  public:
    QueryTableDataDetailHandler() { command = Protocol::REQ_RES_TABLE_DATA_DETAIL; }
    ~QueryTableDataDetailHandler() override = default;
    bool HandleRequest(std::unique_ptr<Dic::Protocol::Request> requestPtr) override;
    static void ComputeTableDetail(const TableDataDetailRequest &request, TableDataDetailResponse &response,
        std::shared_ptr<VirtualTraceDatabase> databasePtr);

    void ComputeLinkPageDetail(TableDataDetailRequest &request, TableDataDetailResponse &response,
        const std::shared_ptr<VirtualTraceDatabase> &database) const;
};
} // namespace Dic::Module::IE
#endif // PROFILER_SERVER_QUERYTABLEDATADETAILHANDLER_H
