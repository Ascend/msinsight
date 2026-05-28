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

#ifndef PROFILER_SERVER_SYSTEMVIEWOVERALLREPOINTERFACE_H
#define PROFILER_SERVER_SYSTEMVIEWOVERALLREPOINTERFACE_H

#include "SystemViewOverallHelper.h"
#include "TimelineProtocolRequest.h"
#include "DataBaseManager.h"
using namespace Dic::Protocol;
namespace Dic::Module::Timeline {
class SystemViewOverallRepoInterface {
  public:
    virtual ~SystemViewOverallRepoInterface() = default;
    virtual std::vector<OverallTmpInfo> QueryOverlapAnalysisDataForOverallMetric(
        const Protocol::SystemViewOverallReqParam &requestParams,
        const std::shared_ptr<VirtualTraceDatabase> &database) = 0;
    virtual bool QueryDataForComputingOverallMetric(const Protocol::SystemViewOverallReqParam &requestParams,
        SystemViewOverallHelper &computeHelper, const std::shared_ptr<VirtualTraceDatabase> &database) = 0;
    virtual void QueryCommunicationOverlapOverallInfos(const Protocol::SystemViewOverallReqParam &requestParams,
        SystemViewOverallHelper &overallHelper, std::vector<Protocol::SystemViewOverallRes> &responseBody,
        const std::shared_ptr<VirtualTraceDatabase> &database) = 0;
    virtual bool QueryCommunicationOpsTimeDataByGroupName(const SystemViewOverallReqParam &params, uint64_t offset,
        const std::vector<Protocol::ThreadTraces> &notOverlapData, std::vector<SameOperatorsDetails> &opsDetails,
        const std::shared_ptr<VirtualTraceDatabase> &database) = 0;
};
}

#endif // PROFILER_SERVER_SYSTEMVIEWOVERALLREPOINTERFACE_H
