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

#ifndef PROFILER_SERVER_SYSTEMVIEWOVERALLDBREPO_H
#define PROFILER_SERVER_SYSTEMVIEWOVERALLDBREPO_H

#include <mutex>
#include "SystemViewOverallRepoInterface.h"
namespace Dic::Module::Timeline {

struct BindParamsForGMAndCS {
    int deviceId;
    SystemViewOverallHelper &overallHelper;
    const Protocol::SystemViewOverallReqParam &requestParams;
};

class SystemViewOverallDbRepo : public SystemViewOverallRepoInterface {
  public:
    std::vector<OverallTmpInfo> QueryOverlapAnalysisDataForOverallMetric(
        const Protocol::SystemViewOverallReqParam &requestParams,
        const std::shared_ptr<VirtualTraceDatabase> &database) override;
    bool QueryDataForComputingOverallMetric(const Protocol::SystemViewOverallReqParam &requestParams,
        SystemViewOverallHelper &computeHelper, const std::shared_ptr<VirtualTraceDatabase> &database) override;
    void QueryCommunicationOverlapOverallInfos(const Protocol::SystemViewOverallReqParam &requestParams,
        SystemViewOverallHelper &overallHelper, std::vector<Protocol::SystemViewOverallRes> &responseBody,
        const std::shared_ptr<VirtualTraceDatabase> &database) override;
    bool QueryCommunicationOpsTimeDataByGroupName(const SystemViewOverallReqParam &params, uint64_t offset,
        const std::vector<Protocol::ThreadTraces> &notOverlapData, std::vector<SameOperatorsDetails> &opsDetails,
        const std::shared_ptr<VirtualTraceDatabase> &database) override;

  private:
    static void QueryGroupMapAndCalculateSummary(const std::shared_ptr<VirtualTraceDatabase> &database,
        std::vector<Protocol::SystemViewOverallRes> &responseBody,
        std::vector<Protocol::SystemViewOverallRes>::iterator it, const std::vector<Protocol::ThreadTraces> &uncovered,
        BindParamsForGMAndCS bindParamsForGmAndCs);
    static bool CheckDataForSystemViewOverall(const std::shared_ptr<VirtualTraceDatabase> &database);
    static bool GetTmpTableForOverall(const std::shared_ptr<VirtualTraceDatabase> &database);
    static std::map<uint64_t, uint64_t> QueryFlowDict(const Protocol::SystemViewOverallReqParam &requestParams,
        const std::shared_ptr<VirtualTraceDatabase> &database, int deviceId);
    std::vector<CpuCubeOpInfo> QueryCpuCubeOp(const Protocol::SystemViewOverallReqParam &requestParams,
        const std::shared_ptr<VirtualTraceDatabase> &database);
    std::vector<OverallTmpInfo> QueryKernelEventsForSystemViewOverall(
        const Protocol::SystemViewOverallReqParam &requestParams, const std::shared_ptr<VirtualTraceDatabase> &database,
        const std::map<uint64_t, uint64_t> &flowDict, int deviceId);
    static void QueryBwdTrackIdForComputingOverall(
        const std::shared_ptr<VirtualTraceDatabase> &database, uint64_t &bwdTrackId);
    std::string GetOrUpdateStringCacheValue(
        const std::shared_ptr<VirtualTraceDatabase> &database, const std::string &path, const std::string &key);
    void UpdateStringCacheValue(const std::shared_ptr<VirtualTraceDatabase> &database, const std::string &path);
    std::map<std::string, std::map<std::string, std::string>> stringsCache;
    std::recursive_mutex mutex;
};
}

#endif // PROFILER_SERVER_SYSTEMVIEWOVERALLDBREPO_H
