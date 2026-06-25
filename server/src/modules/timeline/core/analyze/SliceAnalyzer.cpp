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
#include "pch.h"
#include "SliceAnalyzer.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
namespace Dic::Module::Timeline {
namespace {
const std::string TEXT_PYTHON_FUNCTION_CAT = "python_function";
const std::string PYTHON_STACK_CACHE_SUFFIX = "@python_stack";
const size_t INVALID_UNIT_INDEX = std::numeric_limits<size_t>::max();

struct SliceDepthUnit {
    SliceInterval interval;
};

class PythonFunctionFilter {
  public:
    explicit PythonFunctionFilter(const std::vector<uint64_t> &pythonFunctionIds) {
        filteredIds.reserve(pythonFunctionIds.size());
        for (uint64_t id : pythonFunctionIds) {
            filteredIds.insert(id);
        }
    }

    bool Contains(uint64_t id) const { return !filteredIds.empty() && filteredIds.find(id) != filteredIds.end(); }

  private:
    std::unordered_set<uint64_t> filteredIds;
};

std::string BuildPythonStackCacheKey(uint64_t trackId) { return std::to_string(trackId) + PYTHON_STACK_CACHE_SUFFIX; }

bool IsFilteredPythonFunction(const std::vector<uint64_t> &pythonFunctionIds, uint64_t id) {
    return !std::empty(pythonFunctionIds) && std::binary_search(pythonFunctionIds.begin(), pythonFunctionIds.end(), id);
}

SliceQuery BuildDepthIndexQuery(SliceCacheManager &sliceCacheManager, const SliceQuery &sliceQuery,
    const std::string &sliceCacheKey, const SliceQuery &slicePagedQuery, bool isHitCache) {
    if (!isHitCache) {
        return slicePagedQuery;
    }
    SliceQuery depthIndexQuery = sliceQuery;
    uint64_t cacheStartTime = 0;
    uint64_t cacheEndTime = 0;
    if (sliceCacheManager.QueryCacheDuration(sliceCacheKey, sliceQuery.rankId, cacheStartTime, cacheEndTime)) {
        depthIndexQuery.startTime = cacheStartTime;
        depthIndexQuery.endTime = cacheEndTime;
    }
    return depthIndexQuery;
}

bool AreIntervalsOverlap(const SliceInterval &left, const SliceInterval &right) {
    return left.startTime < right.endTime && right.startTime < left.endTime;
}

bool IsUnitAvailableOnDepth(const std::vector<SliceInterval> &depthIntervals, const SliceInterval &targetInterval) {
    for (const auto &current : depthIntervals) {
        if (AreIntervalsOverlap(current, targetInterval)) {
            return false;
        }
    }
    return true;
}

uint32_t AssignUnitDepthsByIntervalScan(const std::vector<SliceDepthUnit> &units, std::vector<uint32_t> &unitDepths) {
    std::vector<std::vector<SliceInterval>> depthIntervals;
    for (size_t i = 0; i < units.size(); ++i) {
        uint32_t depth = 0;
        while (depth < depthIntervals.size() && !IsUnitAvailableOnDepth(depthIntervals[depth], units[i].interval)) {
            ++depth;
        }
        if (depth == depthIntervals.size()) {
            depthIntervals.emplace_back();
        }
        unitDepths[i] = depth;
        depthIntervals[depth].emplace_back(units[i].interval);
    }
    return static_cast<uint32_t>(depthIntervals.size());
}

uint32_t AssignUnitDepthsByHeap(const std::vector<SliceDepthUnit> &units, std::vector<uint32_t> &unitDepths) {
    using BusyDepth = std::pair<uint64_t, uint32_t>;
    std::priority_queue<BusyDepth, std::vector<BusyDepth>, std::greater<BusyDepth>> busyDepths;
    std::set<uint32_t> reusableDepths;
    uint32_t nextDepth = 0;
    for (size_t i = 0; i < units.size(); ++i) {
        const SliceInterval &interval = units[i].interval;
        while (!busyDepths.empty() && busyDepths.top().first <= interval.startTime) {
            reusableDepths.insert(busyDepths.top().second);
            busyDepths.pop();
        }
        uint32_t depth = 0;
        if (reusableDepths.empty()) {
            depth = nextDepth++;
        } else {
            auto depthIt = reusableDepths.begin();
            depth = *depthIt;
            reusableDepths.erase(depthIt);
        }
        unitDepths[i] = depth;
        busyDepths.emplace(interval.endTime, depth);
    }
    return nextDepth;
}

void BackfillSliceDepths(std::vector<SliceDomain> &sliceDomain, const std::vector<size_t> &sliceToUnit,
    const std::vector<uint32_t> &unitDepths) {
    for (size_t i = 0; i < sliceDomain.size(); ++i) {
        size_t unitIndex = sliceToUnit[i];
        if (unitIndex == INVALID_UNIT_INDEX) {
            continue;
        }
        sliceDomain[i].depth = unitDepths[unitIndex];
    }
}
}

SliceAnalyzer::SliceAnalyzer() {
    if (repository == nullptr) {
        repository = std::make_shared<TextRepository>();
    }
};

void SliceAnalyzer::SetRepository(std::shared_ptr<IBaseSliceRepo> repositoryDependency) {
    repository = repositoryDependency;
}

SliceAnalyzer::~SliceAnalyzer() {
    if (repository != nullptr) {
        repository = nullptr;
    }
};

std::set<std::pair<uint64_t, uint32_t>> SliceAnalyzer::ComputeResultIds(uint64_t startTime, uint64_t endTime,
    std::vector<SliceDomain> &sliceDomain, std::vector<DepthHelper> &endList,
    const std::vector<uint64_t> &pythonFunctionIds) {
    // 根据开始时间结束时间把屏幕平均分成1000份
    const int maxDataCount = 1000;
    uint64_t unitTime = (endTime - startTime) / maxDataCount; // 前端传入做了校验保证 startTime <= endTime
    if (unitTime == 0) {
        return ComputeSmallScreenIds(startTime, endTime, sliceDomain, endList, pythonFunctionIds);
    }
    uint32_t maxDepth = AssignSliceDepths(sliceDomain, pythonFunctionIds);
    endList.assign(maxDepth, DepthHelper{});
    for (auto &item : endList) {
        item.curLimitTime = startTime + unitTime; // (startTime + unitTime) < endTime < UINT64_MAX
    }
    std::set<std::pair<uint64_t, uint32_t>> ids;
    for (auto &item : sliceDomain) {
        if (IsFilteredPythonFunction(pythonFunctionIds, item.id)) {
            continue;
        }
        // 不在屏幕中的算子只参与深度计算，不参与采样过程
        if (!(item.endTime >= startTime && item.timestamp <= endTime)) {
            continue;
        }
        DepthHelper &depthHelper = endList[item.depth];
        // 算子开始时间大于当前份屏幕时间，则把tempId加进结果集，重置tempId，进入下一份屏幕采样
        if (item.timestamp > depthHelper.curLimitTime && depthHelper.curLimitTime <= endTime) {
            ids.emplace(depthHelper.tempId, item.depth);
            depthHelper.tempId = 0;
            depthHelper.tempDuration = 0;
            // item.timestamp 从数据库得到，item.timestamp <= INT64_MAX，unitTime <= UINT64_MAX / 1000
            // item.timestamp + unitTime < UINT64_MAX
            depthHelper.curLimitTime = item.timestamp + unitTime;
        }
        // 更新tempId
        if (item.endTime >= item.timestamp && depthHelper.tempDuration <= item.endTime - item.timestamp) {
            depthHelper.tempId = item.id;
            depthHelper.tempDuration = item.endTime - item.timestamp;
        }
    }
    for (size_t i = 0; i < endList.size(); ++i) {
        ids.emplace(endList[i].tempId, i);
    }
    return ids;
}

/**
 * 屏幕范围小于1000ns的计算方式
 * @param startTime
 * @param endTime
 * @param sliceDomain
 * @param endList
 * @param pythonFunctionIds
 * @return
 */
std::set<std::pair<uint64_t, uint32_t>> SliceAnalyzer::ComputeSmallScreenIds(uint64_t startTime, uint64_t endTime,
    std::vector<SliceDomain> &sliceDomain, std::vector<DepthHelper> &endList,
    const std::vector<uint64_t> &pythonFunctionIds) {
    uint32_t maxDepth = AssignSliceDepths(sliceDomain, pythonFunctionIds);
    endList.assign(maxDepth, DepthHelper{});
    std::set<std::pair<uint64_t, uint32_t>> ids;
    for (auto &item : sliceDomain) {
        if (IsFilteredPythonFunction(pythonFunctionIds, item.id)) {
            continue;
        }
        if (item.endTime >= startTime && item.timestamp <= endTime) {
            ids.emplace(item.id, item.depth);
        }
    }
    return ids;
}

SliceInterval SliceAnalyzer::ToInterval(const SliceDomain &slice) {
    SliceInterval interval;
    interval.startTime = slice.timestamp;
    interval.endTime = slice.endTime >= slice.timestamp ? slice.endTime : slice.timestamp;
    return interval;
}

uint32_t SliceAnalyzer::AssignSliceDepths(
    std::vector<SliceDomain> &sliceDomain, const std::vector<uint64_t> &pythonFunctionIds) {
    std::vector<SliceDepthUnit> units;
    std::vector<size_t> sliceToUnit(sliceDomain.size(), INVALID_UNIT_INDEX);
    std::unordered_map<std::string, size_t> groupToUnit;
    PythonFunctionFilter filter(pythonFunctionIds);
    bool areUnitsOrderedByStartTime = true;

    units.reserve(sliceDomain.size());
    for (size_t i = 0; i < sliceDomain.size(); ++i) {
        if (filter.Contains(sliceDomain[i].id)) {
            continue;
        }
        const SliceInterval interval = ToInterval(sliceDomain[i]);
        const std::string &gid = sliceDomain[i].groupId;
        if (gid.empty()) {
            if (!units.empty() && interval.startTime < units.back().interval.startTime) {
                areUnitsOrderedByStartTime = false;
            }
            sliceToUnit[i] = units.size();
            units.emplace_back(SliceDepthUnit{interval});
            continue;
        }
        if (groupToUnit.empty()) {
            groupToUnit.reserve(sliceDomain.size() - i);
        }
        auto [it, inserted] = groupToUnit.emplace(gid, units.size());
        if (inserted) {
            if (!units.empty() && interval.startTime < units.back().interval.startTime) {
                areUnitsOrderedByStartTime = false;
            }
            units.emplace_back(SliceDepthUnit{interval});
        } else {
            SliceInterval &merged = units[it->second].interval;
            if (interval.startTime < merged.startTime) {
                areUnitsOrderedByStartTime = false;
            }
            merged.startTime = std::min(merged.startTime, interval.startTime);
            merged.endTime = std::max(merged.endTime, interval.endTime);
        }
        sliceToUnit[i] = it->second;
    }

    std::vector<uint32_t> unitDepths(units.size(), 0);
    uint32_t maxDepth = areUnitsOrderedByStartTime ? AssignUnitDepthsByHeap(units, unitDepths)
                                                   : AssignUnitDepthsByIntervalScan(units, unitDepths);
    BackfillSliceDepths(sliceDomain, sliceToUnit, unitDepths);
    return maxDepth;
}

void SliceAnalyzer::SortByTimestampASC(std::vector<SliceDomain> &cacheSlices) {
    std::sort(cacheSlices.begin(), cacheSlices.end(), SliceAnalyzer::CompareTimestampASC);
}

uint32_t SliceAnalyzer::ComputeFlowPointDepth(
    std::vector<SliceDomain> &cacheSlices, std::string &type, uint64_t timestamp) {
    SliceDomain cacheSlice;
    cacheSlice.timestamp = timestamp;
    cacheSlice.id = 0;
    if (type == Protocol::LINE_START) {
        auto it = std::lower_bound(cacheSlices.begin(), cacheSlices.end(), cacheSlice, CompareTimestampASC);
        if (it != cacheSlices.end() && it->timestamp == timestamp) {
            return it->depth;
        }

        while (it != cacheSlices.end() && it > cacheSlices.begin()) {
            it--;
            if (it->timestamp <= timestamp && it->endTime >= timestamp) {
                break;
            }
        }
        if (it == cacheSlices.end()) {
            return 0;
        }
        return it->depth;
    }
    if (type == Protocol::LINE_END || type == Protocol::LINE_END_OPTIONAL) {
        auto it = std::lower_bound(cacheSlices.begin(), cacheSlices.end(), cacheSlice, CompareTimestampASC);
        if (it != cacheSlices.end()) {
            return it->depth;
        }
    }
    return 0;
}

/**
 * 计算每个算子自身执行时间
 * @param rows 所有算子
 * @param selfTimeKeyValue 计算结果
 */
void SliceAnalyzer::CalculateSelfTime(
    std::vector<CompeteSliceDomain> &rows, std::map<std::string, uint64_t> &selfTimeKeyValue) {
    size_t length = rows.size();
    // offset变量用来优化性能
    uint64_t offset = 0;
    for (size_t i = 0; i < length; i++) {
        uint32_t curDepth = rows[i].depth;
        uint64_t selfTime = rows[i].duration;
        uint64_t curSliceStartTime = rows[i].timestamp;
        uint64_t curSliceEndTime = rows[i].endTime;
        for (uint64_t j = offset; j < length; ++j) {
            if (j == length - 1 && rows[j].depth == curDepth) {
                offset = length;
                continue;
            }
            if (rows[j].depth < curDepth + 1) {
                continue;
            }
            if (rows[j].depth > curDepth + 1) {
                offset = j;
                break;
            }
            if (rows[j].timestamp < curSliceStartTime) {
                continue;
            }
            if (rows[j].endTime > curSliceEndTime) {
                offset = j;
                break;
            }
            if (selfTime >= rows[j].duration) {
                selfTime = selfTime - rows[j].duration;
            } else {
                selfTime = 0;
            }
            offset = j;
        }
        AddData(selfTimeKeyValue, rows[i].name, selfTime);
    }
}

void SliceAnalyzer::ComputeScreenSliceIds(
    const SliceQuery &sliceQuery, std::set<uint64_t> &ids, uint64_t &maxDepth, std::map<uint64_t, uint32_t> &depthMap) {
    std::string sliceCacheKey = std::to_string(sliceQuery.trackId);
    auto &instance = SliceCacheManager::Instance();
    std::vector<SliceDomain> sliceDomainVec = instance.GetSliceDomainVec(sliceCacheKey, sliceQuery.rankId, sliceQuery);
    // 用于分页缓存的查询参数, 只有未命中缓存时，会被赋值；命中缓存时，其为空值，可以作为后续是否刷新cacheDuration的判断依据
    SliceQuery slicePagedQuery;
    bool isHitCache = !std::empty(sliceDomainVec);
    if (!isHitCache) {
        slicePagedQuery = SliceCacheManager::GetSlicePagedQuery(sliceQuery);
        repository->QuerySimpleSliceWithOutNameByTrackId(slicePagedQuery, sliceDomainVec);
    }
    SliceQuery pythonFunctionQuery = sliceQuery;
    if (pythonFunctionQuery.cat.empty()) {
        pythonFunctionQuery.cat = TEXT_PYTHON_FUNCTION_CAT;
    }
    std::vector<uint64_t> pythonFunctionIds;
    QueryPythonFuncIds(pythonFunctionQuery, pythonFunctionIds);
    std::vector<DepthHelper> endList;
    std::set<std::pair<uint64_t, uint32_t>> idPairVec = ComputeResultIds(sliceQuery.startTime + sliceQuery.minTimestamp,
        sliceQuery.endTime + sliceQuery.minTimestamp, sliceDomainVec, endList, pythonFunctionIds);
    for (const auto &item : idPairVec) {
        ids.emplace(item.first);
        depthMap[item.first] = item.second;
    }
    bool isNeedFilterIds = true;
    for (const auto &item : sliceDomainVec) {
        if (item.id == 0) {
            isNeedFilterIds = false;
            break;
        }
    }
    if (isNeedFilterIds) {
        ids.erase(0);
    }
    maxDepth = endList.size();
    // 此处不管是否命中缓存，都需要刷新，是因为depth信息可能会被更新, 而QueryDepthInfo会查询缓存中的depth信息
    instance.UpdateSliceCache(sliceCacheKey, sliceDomainVec, slicePagedQuery);
}

void SliceAnalyzer::ComputePythonFunctionSliceIds(
    const SliceQuery &sliceQuery, std::set<uint64_t> &ids, uint64_t &maxDepth, std::map<uint64_t, uint32_t> &depthMap) {
    std::string sliceCacheKey = std::to_string(sliceQuery.trackId);
    auto &instance = SliceCacheManager::Instance();
    std::vector<SliceDomain> sliceDomainVec = instance.GetSliceDomainVec(sliceCacheKey, sliceQuery.rankId, sliceQuery);
    SliceQuery slicePagedQuery;
    bool isHitCache = !std::empty(sliceDomainVec);
    if (!isHitCache) {
        slicePagedQuery = SliceCacheManager::GetSlicePagedQuery(sliceQuery);
        repository->QuerySimpleSliceWithOutNameByTrackId(slicePagedQuery, sliceDomainVec);
    }

    SliceQuery pythonFunctionQuery = sliceQuery;
    if (pythonFunctionQuery.cat.empty()) {
        pythonFunctionQuery.cat = TEXT_PYTHON_FUNCTION_CAT;
    }
    std::vector<uint64_t> pythonFunctionIds;
    QueryPythonFuncIds(pythonFunctionQuery, pythonFunctionIds);

    std::vector<SliceDomain> pythonSlices;
    for (auto &item : sliceDomainVec) {
        if (std::binary_search(pythonFunctionIds.begin(), pythonFunctionIds.end(), item.id)) {
            pythonSlices.emplace_back(item);
        }
    }

    if (std::empty(pythonSlices)) {
        maxDepth = 0;
        return;
    }

    std::vector<DepthHelper> endList;
    std::set<std::pair<uint64_t, uint32_t>> idPairVec = ComputeResultIds(sliceQuery.startTime + sliceQuery.minTimestamp,
        sliceQuery.endTime + sliceQuery.minTimestamp, pythonSlices, endList, {});
    SliceQuery depthIndexQuery = BuildDepthIndexQuery(instance, sliceQuery, sliceCacheKey, slicePagedQuery, isHitCache);
    instance.UpdateDepthIndexCache(BuildPythonStackCacheKey(sliceQuery.trackId), pythonSlices, depthIndexQuery);
    for (const auto &item : idPairVec) {
        ids.emplace(item.first);
        depthMap[item.first] = item.second;
    }
    if (ids.count(0) > 0) {
        ids.erase(0);
    }
    maxDepth = endList.size();
}

void SliceAnalyzer::QueryPythonFuncIds(const SliceQuery &sliceQuery, std::vector<uint64_t> &pythonFunctionIds) {
    auto &instance = SliceCacheManager::Instance();
    std::string sliceCacheKey = std::to_string(sliceQuery.trackId);
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(repository.get());
    if (instance.GetPythonFunctionStatus(sliceQuery.trackId) == PYTHON_FUNCTION_STATUS::UNKNOWN) {
        uint64_t count = pythonFuncRepo != nullptr ? pythonFuncRepo->QueryPythonFunctionCountByTrackId(sliceQuery) : 0;
        PYTHON_FUNCTION_STATUS status = count == 0 ? PYTHON_FUNCTION_STATUS::NOT_EXIST : PYTHON_FUNCTION_STATUS::EXIST;
        instance.SetPythonFunctionStatus(sliceQuery.trackId, status);
    }
    if (instance.GetPythonFunctionStatus(sliceQuery.trackId) == PYTHON_FUNCTION_STATUS::EXIST) {
        pythonFunctionIds = instance.GetPythonFunctionIdVec(sliceCacheKey, sliceQuery);
        if (std::empty(pythonFunctionIds)) {
            QueryPythonFuncFromDBAndUpdateCache(sliceCacheKey, sliceQuery, pythonFunctionIds);
        }
    }
}

void SliceAnalyzer::ComputeSliceDomainVecAndSelfTimeByTimeRange(const SliceQuery &sliceQuery,
    std::vector<CompeteSliceDomain> &sliceDomainVec, std::map<std::string, uint64_t> &selfTimeKeyValue,
    bool isPythonStack) {
    std::vector<CompeteSliceDomain> allCompeteSliceVec;
    const auto textRepo = dynamic_cast<ITextSlice *>(repository.get());
    if (textRepo == nullptr) {
        return;
    }
    // 查询符合条件的所有算子
    textRepo->QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, allCompeteSliceVec);
    if (std::empty(allCompeteSliceVec)) {
        return;
    }
    // 过滤python function
    std::vector<CompeteSliceDomain> competeSliceVec;
    std::string sliceCacheKey = std::to_string(sliceQuery.trackId);
    auto &instance = SliceCacheManager::Instance();
    SliceQuery pythonFunctionQuery = sliceQuery;
    if (pythonFunctionQuery.cat.empty()) {
        pythonFunctionQuery.cat = TEXT_PYTHON_FUNCTION_CAT;
    }
    std::vector<uint64_t> pythonFunctionIds = instance.GetPythonFunctionIdVec(sliceCacheKey, pythonFunctionQuery);
    if (std::empty(pythonFunctionIds)) {
        QueryPythonFuncFromDBAndUpdateCache(sliceCacheKey, pythonFunctionQuery, pythonFunctionIds);
    }
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    if (isPythonStack) {
        ComputePythonFunctionDepthInfoByTrackId(sliceQuery, depthInfo);
    } else {
        ComputeDepthInfoByTrackId(sliceQuery, depthInfo);
    }
    // 普通泳道过滤 python function；Python Stack 泳道只保留 python function。
    for (auto &item : allCompeteSliceVec) {
        bool isPythonFunction = std::binary_search(pythonFunctionIds.begin(), pythonFunctionIds.end(), item.id);
        if (isPythonFunction != isPythonStack) {
            continue;
        }
        item.depth = depthInfo[item.id];
        item.tid = sliceQuery.tid;
        item.pid = sliceQuery.pid;
        competeSliceVec.emplace_back(std::move(item));
    }
    // 需要先排序再计算SelfTime
    std::sort(competeSliceVec.begin(), competeSliceVec.end(), std::less<CompeteSliceDomain>());
    CalculateSelfTime(competeSliceVec, selfTimeKeyValue);
    uint64_t end = sliceQuery.endTime + sliceQuery.minTimestamp;
    uint64_t start = sliceQuery.startTime + sliceQuery.minTimestamp;
    uint32_t startDepth = NumberUtil::StringToUint32(sliceQuery.startDepth);
    uint32_t endDepth = NumberUtil::StringToUint32(sliceQuery.endDepth);
    for (auto &row : competeSliceVec) {
        if (sliceQuery.startDepth.empty() && sliceQuery.endDepth.empty()) {
            if (row.timestamp <= end && row.endTime >= start) {
                sliceDomainVec.emplace_back(row);
            }
        } else {
            if (row.timestamp <= end && row.endTime >= start && row.depth >= startDepth && row.depth <= endDepth) {
                sliceDomainVec.emplace_back(row);
            }
        }
    }
}

void SliceAnalyzer::ComputeDepthInfoByTrackId(
    const SliceQuery &sliceQuery, std::unordered_map<uint64_t, uint32_t> &depthInfo) {
    SliceCacheManager &sliceCacheManager = SliceCacheManager::Instance();
    bool cacheIsExist = sliceCacheManager.QueryDepthInfo(depthInfo, sliceQuery);
    if (!cacheIsExist) {
        ComputeDepthInfoFromDB(sliceQuery, depthInfo);
    }
}

void SliceAnalyzer::ComputePythonFunctionDepthInfoByTrackId(
    const SliceQuery &sliceQuery, std::unordered_map<uint64_t, uint32_t> &depthInfo) {
    SliceCacheManager &sliceCacheManager = SliceCacheManager::Instance();
    std::string sliceCacheKey = std::to_string(sliceQuery.trackId);
    std::vector<SliceDomain> sliceVec =
        sliceCacheManager.GetSliceDomainVec(sliceCacheKey, sliceQuery.rankId, sliceQuery);
    if (std::empty(sliceVec)) {
        SliceQuery slicePagedQuery = SliceCacheManager::GetSlicePagedQuery(sliceQuery);
        repository->QuerySimpleSliceWithOutNameByTrackId(slicePagedQuery, sliceVec);
    }

    SliceQuery pythonFunctionQuery = sliceQuery;
    if (pythonFunctionQuery.cat.empty()) {
        pythonFunctionQuery.cat = TEXT_PYTHON_FUNCTION_CAT;
    }
    std::vector<uint64_t> pythonFunctionIds;
    QueryPythonFuncIds(pythonFunctionQuery, pythonFunctionIds);
    if (std::empty(pythonFunctionIds)) {
        return;
    }

    std::vector<SliceDomain> pythonSlices;
    pythonSlices.reserve(sliceVec.size());
    for (auto &item : sliceVec) {
        if (!std::binary_search(pythonFunctionIds.begin(), pythonFunctionIds.end(), item.id)) {
            continue;
        }
        pythonSlices.emplace_back(item);
    }
    AssignSliceDepths(pythonSlices, {});
    for (auto &item : pythonSlices) {
        depthInfo[item.id] = item.depth;
    }
    SliceQuery depthIndexQuery = sliceQuery;
    uint64_t cacheStartTime = 0;
    uint64_t cacheEndTime = 0;
    if (sliceCacheManager.QueryCacheDuration(sliceCacheKey, sliceQuery.rankId, cacheStartTime, cacheEndTime)) {
        depthIndexQuery.startTime = cacheStartTime;
        depthIndexQuery.endTime = cacheEndTime;
    }
    sliceCacheManager.UpdateDepthIndexCache(
        BuildPythonStackCacheKey(sliceQuery.trackId), pythonSlices, depthIndexQuery);
}

void SliceAnalyzer::ComputePythonFunctionSliceVecByTimeRange(
    const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    SliceQuery pythonFunctionQuery = sliceQuery;
    if (pythonFunctionQuery.cat.empty()) {
        pythonFunctionQuery.cat = TEXT_PYTHON_FUNCTION_CAT;
    }
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(repository.get());
    if (pythonFuncRepo == nullptr) {
        return;
    }
    if (pythonFuncRepo->QuerySliceByCatAndTimeRange(pythonFunctionQuery, sliceVec)) {
        if (std::empty(sliceVec)) {
            return;
        }
        SortByTimestampASC(sliceVec);
        AssignSliceDepths(sliceVec, {});
        return;
    }

    std::vector<SliceDomain> allSlices;
    repository->QuerySimpleSliceWithOutNameByTrackId(sliceQuery, allSlices);
    if (std::empty(allSlices)) {
        return;
    }

    std::vector<uint64_t> pythonFunctionIds;
    pythonFuncRepo->QuerySliceIdsByCat(pythonFunctionQuery, pythonFunctionIds);
    if (std::empty(pythonFunctionIds)) {
        return;
    }
    std::sort(pythonFunctionIds.begin(), pythonFunctionIds.end());

    sliceVec.reserve(allSlices.size());
    for (auto &item : allSlices) {
        if (!std::binary_search(pythonFunctionIds.begin(), pythonFunctionIds.end(), item.id)) {
            continue;
        }
        sliceVec.emplace_back(item);
    }
    SortByTimestampASC(sliceVec);
    AssignSliceDepths(sliceVec, {});
}

void SliceAnalyzer::ComputeSliceDomainVecByTrackId(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    SliceCacheManager &sliceCacheManager = SliceCacheManager::Instance();
    sliceVec = sliceCacheManager.GetSliceDomainVec(std::to_string(sliceQuery.trackId), sliceQuery.rankId, sliceQuery);
    if (std::empty(sliceVec)) {
        std::unordered_map<uint64_t, uint32_t> depthInfo;
        ComputeDepthInfoFromDB(sliceQuery, depthInfo);
        sliceVec =
            sliceCacheManager.GetSliceDomainVec(std::to_string(sliceQuery.trackId), sliceQuery.rankId, sliceQuery);
    }
}

void SliceAnalyzer::QueryPythonFuncFromDBAndUpdateCache(
    const std::string &key, const SliceQuery &sliceQuery, std::vector<uint64_t> &pythonFunctionIds) {
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(repository.get());
    SliceCacheManager &sliceCache = SliceCacheManager::Instance();
    SliceQuery slicePagedQuery = SliceCacheManager::GetSlicePagedQuery(sliceQuery);
    if (pythonFuncRepo != nullptr) {
        pythonFuncRepo->QuerySliceIdsByCat(slicePagedQuery, pythonFunctionIds);
    }
    sliceCache.PutPythonFunctionIdVec(key, pythonFunctionIds, slicePagedQuery);
}

void SliceAnalyzer::ComputeDepthInfoFromDB(
    const SliceQuery &sliceQuery, std::unordered_map<uint64_t, uint32_t> &depthInfo) {
    std::vector<SliceDomain> sliceVec;
    SliceCacheManager &simpleSliceCache = SliceCacheManager::Instance();
    std::string pythonFunctionKey = std::to_string(sliceQuery.trackId);
    std::vector<uint64_t> pythonFunctionIds = simpleSliceCache.GetPythonFunctionIdVec(pythonFunctionKey, sliceQuery);
    if (sliceQuery.isFilterPythonFunction && std::empty(pythonFunctionIds)) {
        QueryPythonFuncFromDBAndUpdateCache(pythonFunctionKey, sliceQuery, pythonFunctionIds);
    }
    SliceQuery slicePagedQuery = SliceCacheManager::GetSlicePagedQuery(sliceQuery);
    repository->QuerySimpleSliceWithOutNameByTrackId(slicePagedQuery, sliceVec);
    AssignSliceDepths(sliceVec, pythonFunctionIds);
    for (auto &item : sliceVec) {
        if (IsFilteredPythonFunction(pythonFunctionIds, item.id)) {
            continue;
        }
        depthInfo[item.id] = item.depth;
    }
    simpleSliceCache.UpdateSliceCache(std::to_string(sliceQuery.trackId), sliceVec, slicePagedQuery);
}

void SliceAnalyzer::AddData(
    std::map<std::string, uint64_t> &selfTimeKeyValue, const std::string &name, uint64_t tmpSelfTime) {
    if (selfTimeKeyValue.find(name) != selfTimeKeyValue.end()) {
        selfTimeKeyValue.at(name) = selfTimeKeyValue.at(name) + tmpSelfTime;
    } else {
        selfTimeKeyValue.emplace(name, tmpSelfTime);
    }
}

bool SliceAnalyzer::CompareTimestampASC(const SliceDomain &first, const SliceDomain &second) {
    if (first.timestamp < second.timestamp) {
        return true;
    }
    if (first.timestamp == second.timestamp && first.id < second.id) {
        return true;
    }
    return false;
}

void SliceAnalyzer::ComputeAllThreadInfo(
    const ThreadQuery &flowQuery, std::unordered_map<uint64_t, std::pair<std::string, std::string>> &threadInfo) {
    const auto textRepo = dynamic_cast<ITextSlice *>(repository.get());
    if (textRepo != nullptr) {
        textRepo->QueryAllThreadInfo(flowQuery, threadInfo);
    }
}
}
