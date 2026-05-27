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

#ifndef PROFILER_SERVER_SLICEANALYZER_H
#define PROFILER_SERVER_SLICEANALYZER_H
#include <set>
#include "TimelineProtocolResponse.h"
#include "DominQuery.h"
#include "DomainObject.h"
#include "TextRepository.h"
#include "DataEngineInterface.h"
#include "SliceCacheManager.h"
namespace Dic::Module::Timeline {
struct DepthHelper {
    uint64_t endTime = 0;
    uint64_t tempId = 0;
    uint64_t tempDuration = 0;
    uint64_t curLimitTime = 0;
};
class SliceAnalyzer {
  public:
    SliceAnalyzer();
    ~SliceAnalyzer();
    static void SortByTimestampASC(std::vector<SliceDomain> &cacheSlices);
    static uint32_t ComputeFlowPointDepth(std::vector<SliceDomain> &cacheSlices, std::string &type, uint64_t timestamp);
    void ComputeScreenSliceIds(const SliceQuery &sliceQuery, std::set<uint64_t> &ids, uint64_t &maxDepth,
        std::map<uint64_t, uint32_t> &depthMap);
    void ComputePythonFunctionSliceIds(const SliceQuery &sliceQuery, std::set<uint64_t> &ids, uint64_t &maxDepth,
        std::map<uint64_t, uint32_t> &depthMap);
    /* *
     * 计算在时间范围内和与时间范围相交的算子信息
     */
    void ComputeSliceDomainVecAndSelfTimeByTimeRange(const SliceQuery &sliceQuery,
        std::vector<CompeteSliceDomain> &sliceDomainVec, std::map<std::string, uint64_t> &selfTimeKeyValue,
        bool isPythonStack = false);
    /* *
     * 根据泳道trackId计算泳道下所有深度信息
     * @param trackId
     * @param depthInfo
     */
    void ComputeDepthInfoByTrackId(const SliceQuery &sliceQuery, std::unordered_map<uint64_t, uint32_t> &depthInfo);
    /* *
     * 根据泳道trackId计算泳道下所有简单算子信息
     * @param trackId
     * @param depthInfo
     */
    void ComputeSliceDomainVecByTrackId(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec);
    void ComputeAllThreadInfo(
        const ThreadQuery &flowQuery, std::unordered_map<uint64_t, std::pair<std::string, std::string>> &threadInfo);
    void SetRepository(std::shared_ptr<IBaseSliceRepo> repository);

  private:
    std::shared_ptr<IBaseSliceRepo> repository;
    static bool CompareTimestampASC(const SliceDomain &first, const SliceDomain &second);
    static void AddData(
        std::map<std::string, uint64_t> &selfTimeKeyValue, const std::string &name, uint64_t tmpSelfTime);
    static std::set<std::pair<uint64_t, uint32_t>> ComputeResultIds(uint64_t startTime, uint64_t endTime,
        std::vector<SliceDomain> &sliceDomain, std::vector<DepthHelper> &endList,
        const std::vector<uint64_t> &pythonFunctionIds);
    static std::set<std::pair<uint64_t, uint32_t>> ComputeSmallScreenIds(uint64_t startTime, uint64_t endTime,
        std::vector<SliceDomain> &sliceDomain, std::vector<DepthHelper> &endList,
        const std::vector<uint64_t> &pythonFunctionIds);
    static void CalculateSelfTime(
        std::vector<CompeteSliceDomain> &rows, std::map<std::string, uint64_t> &selfTimeKeyValue);

    void ComputeDepthInfoFromDB(const SliceQuery &sliceQuery, std::unordered_map<uint64_t, uint32_t> &depthInfo);

    void QueryPythonFuncIds(const SliceQuery &sliceQuery, std::vector<uint64_t> &pythonFunctionIds);
    void QueryPythonFuncFromDBAndUpdateCache(
        const std::string &key, const SliceQuery &sliceQuery, std::vector<uint64_t> &pythonFunctionIds);
};
}
#endif // PROFILER_SERVER_SLICEANALYZER_H
