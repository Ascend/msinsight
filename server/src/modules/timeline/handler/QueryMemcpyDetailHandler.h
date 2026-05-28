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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_QUERYMEMCPYDETAILHANDLER_H
#define PROFILER_SERVER_QUERYMEMCPYDETAILHANDLER_H

#include <memory>
#include <string>
#include <vector>

#include "TimelineRequestHandler.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Timeline {

/**
 * 处理Memcpy详细数据查询请求
 * 支持按线程ID、拷贝类型、时间范围过滤
 */
class QueryMemcpyDetailHandler : public TimelineRequestHandler {
  public:
    QueryMemcpyDetailHandler() { command = Protocol::REQ_RES_MEMCPY_DETAIL; }
    ~QueryMemcpyDetailHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    /**
     * 执行数据库查询
     * @param request 查询参数
     * @param response 返回结果
     * @param error 错误信息
     * @param database 数据库连接
     * @return 操作是否成功
     */
    static bool QueryMemcpyDetails(const SystemViewOverallMoreDetailsRequest &request, MemcpyDetailResponse &response,
        std::string &error, const std::shared_ptr<VirtualTraceDatabase> &database);
};

} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_QUERYMEMCPYDETAILHANDLER_H
