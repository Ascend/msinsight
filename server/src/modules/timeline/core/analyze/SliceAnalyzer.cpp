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
namespace Dic::Module::Timeline {
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
    std::set<std::pair<uint64_t, uint32_t>> ids;
    for (auto &item : sliceDomain) {
        if (!std::empty(pythonFunctionIds) &&
            std::binary_search(pythonFunctionIds.begin(), pythonFunctionIds.end(), item.id)) {
            continue;
        }
        for (item.depth = 0; item.depth < endList.size() && endList[item.depth].endTime > item.timestamp;
             item.depth++) {
        }
        if (item.depth < endList.size()) {
            endList[item.depth].endTime = item.endTime;
            // 不在屏幕中的算子只参与深度计算，不参与采样过程
            if (!(item.endTime >= startTime && item.timestamp <= endTime)) {
                continue;
            }
            // 算子开始时间大于当前份屏幕时间，则把tempId加进结果集，重置tempId，进入下一份屏幕采样
            if (item.timestamp > endList[item.depth].curLimitTime && endList[item.depth].curLimitTime <= endTime) {
                ids.emplace(endList[item.depth].tempId, item.depth);
                endList[item.depth].tempId = 0;
                endList[item.depth].tempDuration = 0;
                // item.timestamp 从数据库得到，item.timestamp <= INT64_MAX，unitTime <= UINT64_MAX / 1000
                // item.timestamp + unitTime < UINT64_MAX
                endList[item.depth].curLimitTime = item.timestamp + unitTime;
            }
            // 更新tempId
            if (item.endTime >= item.timestamp && endList[item.depth].tempDuration <= item.endTime - item.timestamp) {
                endList[item.depth].tempId = item.id;
                endList[item.depth].tempDuration = item.endTime - item.timestamp;
            }
        } else {
            DepthHelper depthHelper;
            depthHelper.endTime = item.endTime;
            depthHelper.curLimitTime = startTime + unitTime; // (startTime + unitTime) < endTime < UINT64_MAX
            if (item.endTime >= startTime && item.timestamp <= endTime && item.endTime >= item.timestamp) {
                depthHelper.tempId = item.id;
                depthHelper.tempDuration = item.endTime - item.timestamp;
            }
            endList.emplace_back(depthHelper);
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
    std::set<std::pair<uint64_t, uint32_t>> ids;
    for (auto &item : sliceDomain) {
        if (!std::empty(pythonFunctionIds) &&
            std::binary_search(pythonFunctionIds.begin(), pythonFunctionIds.end(), item.id)) {
            continue;
        }
        for (item.depth = 0; item.depth < endList.size() && endList[item.depth].endTime > item.timestamp;
             item.depth++) {
        }
        if (item.depth < endList.size()) {
            endList[item.depth].endTime = item.endTime;
        } else {
            DepthHelper depthHelper;
            depthHelper.endTime = item.endTime;
            endList.emplace_back(depthHelper);
        }
        if (item.endTime >= startTime && item.timestamp <= endTime) {
            ids.emplace(item.id, item.depth);
        }
    }
    return ids;
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
    std::vector<uint64_t> pythonFunctionIds;
    QueryPythonFuncIds(sliceQuery, pythonFunctionIds);
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

    std::vector<uint64_t> pythonFunctionIds;
    if (instance.GetPythonFunctionStatus(sliceQuery.trackId) == PYTHON_FUNCTION_STATUS::UNKNOWN) {
        const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(repository.get());
        uint64_t count = pythonFuncRepo != nullptr ? pythonFuncRepo->QueryPythonFunctionCountByTrackId(sliceQuery) : 0;
        instance.SetPythonFunctionStatus(
            sliceQuery.trackId, count == 0 ? PYTHON_FUNCTION_STATUS::NOT_EXIST : PYTHON_FUNCTION_STATUS::EXIST);
    }
    if (instance.GetPythonFunctionStatus(sliceQuery.trackId) == PYTHON_FUNCTION_STATUS::EXIST) {
        pythonFunctionIds = instance.GetPythonFunctionIdVec(sliceCacheKey, sliceQuery);
        if (std::empty(pythonFunctionIds)) {
            QueryPythonFuncFromDBAndUpdateCache(sliceCacheKey, sliceQuery, pythonFunctionIds);
        }
    }

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
    std::vector<uint64_t> pythonFunctionIds = instance.GetPythonFunctionIdVec(sliceCacheKey, sliceQuery);
    if (std::empty(pythonFunctionIds)) {
        QueryPythonFuncFromDBAndUpdateCache(sliceCacheKey, sliceQuery, pythonFunctionIds);
    }
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    ComputeDepthInfoByTrackId(sliceQuery, depthInfo);
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
    std::vector<uint64_t> endList;
    for (auto &item : sliceVec) {
        if (std::binary_search(pythonFunctionIds.begin(), pythonFunctionIds.end(), item.id)) {
            continue;
        }
        while (item.depth < endList.size() && endList[item.depth] > item.timestamp) {
            item.depth++;
        }
        if (item.depth < endList.size()) {
            endList[item.depth] = item.endTime;
        } else {
            endList.emplace_back(item.endTime);
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
