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

#ifndef PROFILER_SERVER_SLICEANALYZER_MOCK_DATA_H
#define PROFILER_SERVER_SLICEANALYZER_MOCK_DATA_H

#include <DominQuery.h>
#include "SliceCacheManager.h"
#include "algorithm"
using namespace Dic::Module::Timeline;
namespace Dic::TimeLine::SliceAnalyzer::Mock {
inline void QueryCompeteSliceVecByTimeRangeAndTrackId_mock(
    const Module::Timeline::SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) {
    std::vector<CompeteSliceDomain> temp;
    CompeteSliceDomain competeSliceDomain1 = {1, 2, 20, 22, 0, "slice1", 3, ""};
    CompeteSliceDomain competeSliceDomain2 = {2, 3, 2, 5, 0, "slice2", 3, ""};
    CompeteSliceDomain competeSliceDomain3 = {3, 6, 3, 9, 0, "slice3", 3, ""};
    CompeteSliceDomain competeSliceDomain4 = {4, 10, 9, 19, 0, "slice2", 3, ""};
    CompeteSliceDomain competeSliceDomain5 = {5, 7, 1, 8, 0, "slice5", 3, ""};
    CompeteSliceDomain competeSliceDomain6 = {6, 11, 1, 12, 0, "slice4", 3, ""};
    CompeteSliceDomain competeSliceDomain7 = {7, 13, 1, 14, 0, "slice7", 3, ""};
    CompeteSliceDomain competeSliceDomain8 = {8, 15, 3, 18, 0, "slice3", 3, ""};
    CompeteSliceDomain competeSliceDomain9 = {9, 16, 1, 17, 0, "slice10", 3, ""};
    CompeteSliceDomain competeSliceDomain10 = {10, 16, 1, 17, 0, "slice10", 3, ""};
    temp.emplace_back(competeSliceDomain1);
    temp.emplace_back(competeSliceDomain2);
    temp.emplace_back(competeSliceDomain3);
    temp.emplace_back(competeSliceDomain4);
    temp.emplace_back(competeSliceDomain5);
    temp.emplace_back(competeSliceDomain6);
    temp.emplace_back(competeSliceDomain7);
    temp.emplace_back(competeSliceDomain8);
    temp.emplace_back(competeSliceDomain9);
    temp.emplace_back(competeSliceDomain10);
    for (const auto &item : temp) {
        if (item.timestamp <= sliceQuery.endTime + sliceQuery.minTimestamp &&
            item.endTime >= sliceQuery.startTime + sliceQuery.minTimestamp) {
            sliceVec.emplace_back(item);
        }
    }
};
inline void QuerySimpleSliceWithOutNameByTrackId_mock(
    const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    SliceDomain sliceDomain1 = {1, 2, 22, 0};
    SliceDomain sliceDomain2 = {2, 3, 5, 0};
    SliceDomain sliceDomain3 = {3, 6, 9, 0};
    SliceDomain sliceDomain4 = {4, 10, 19, 0};
    SliceDomain sliceDomain5 = {5, 7, 8, 0};
    SliceDomain sliceDomain6 = {6, 11, 12, 0};
    SliceDomain sliceDomain7 = {7, 13, 14, 0};
    SliceDomain sliceDomain8 = {8, 15, 18, 0};
    SliceDomain sliceDomain9 = {9, 16, 17, 0};
    SliceDomain sliceDomain10 = {10, 16, 17, 0};
    sliceVec.emplace_back(sliceDomain1);
    sliceVec.emplace_back(sliceDomain2);
    sliceVec.emplace_back(sliceDomain3);
    sliceVec.emplace_back(sliceDomain4);
    sliceVec.emplace_back(sliceDomain5);
    sliceVec.emplace_back(sliceDomain6);
    sliceVec.emplace_back(sliceDomain7);
    sliceVec.emplace_back(sliceDomain8);
    sliceVec.emplace_back(sliceDomain9);
    sliceVec.emplace_back(sliceDomain10);
    std::sort(sliceVec.begin(), sliceVec.end(), [](const SliceDomain &first, const SliceDomain &second) {
        if (first.timestamp < second.timestamp) {
            return true;
        }
        return (first.timestamp == second.timestamp) && first.id < second.id;
    });
}

inline void SliceCacheNotFliterPythonMock() {
    SliceDomain sliceDomain1 = {1, 2, 22, 0};
    SliceDomain sliceDomain2 = {2, 3, 5, 1};
    SliceDomain sliceDomain3 = {3, 6, 9, 1};
    SliceDomain sliceDomain4 = {4, 10, 19, 1};
    SliceDomain sliceDomain5 = {5, 7, 8, 2};
    SliceDomain sliceDomain6 = {6, 11, 12, 2};
    SliceDomain sliceDomain7 = {7, 13, 14, 2};
    SliceDomain sliceDomain8 = {8, 15, 18, 2};
    SliceDomain sliceDomain9 = {9, 16, 17, 3};
    SliceDomain sliceDomain10 = {10, 16, 17, 4};
    std::vector<SliceDomain> sliceVec;
    sliceVec.emplace_back(sliceDomain1);
    sliceVec.emplace_back(sliceDomain2);
    sliceVec.emplace_back(sliceDomain3);
    sliceVec.emplace_back(sliceDomain4);
    sliceVec.emplace_back(sliceDomain5);
    sliceVec.emplace_back(sliceDomain6);
    sliceVec.emplace_back(sliceDomain7);
    sliceVec.emplace_back(sliceDomain8);
    sliceVec.emplace_back(sliceDomain9);
    sliceVec.emplace_back(sliceDomain10);
    std::sort(sliceVec.begin(), sliceVec.end(), [](const SliceDomain &first, const SliceDomain &second) {
        if (first.timestamp < second.timestamp) {
            return true;
        }
        return (first.timestamp == second.timestamp) && first.id < second.id;
    });
    SliceQuery sliceQuery = {3, 0, 23, 2};
    SliceCacheManager::Instance().UpdateSliceCache("3", sliceVec, sliceQuery);
}

inline void SliceCacheFliterPythonMock() {
    SliceDomain sliceDomain1 = {1, 2, 22, 0};
    SliceDomain sliceDomain2 = {2, 3, 5, 0};
    SliceDomain sliceDomain3 = {3, 6, 9, 0};
    SliceDomain sliceDomain4 = {4, 10, 19, 0};
    SliceDomain sliceDomain5 = {5, 7, 8, 1};
    SliceDomain sliceDomain6 = {6, 11, 12, 1};
    SliceDomain sliceDomain7 = {7, 13, 14, 1};
    SliceDomain sliceDomain8 = {8, 15, 18, 1};
    SliceDomain sliceDomain9 = {9, 16, 17, 2};
    SliceDomain sliceDomain10 = {10, 16, 17, 3};
    std::vector<SliceDomain> sliceVec;
    sliceVec.emplace_back(sliceDomain1);
    sliceVec.emplace_back(sliceDomain2);
    sliceVec.emplace_back(sliceDomain3);
    sliceVec.emplace_back(sliceDomain4);
    sliceVec.emplace_back(sliceDomain5);
    sliceVec.emplace_back(sliceDomain6);
    sliceVec.emplace_back(sliceDomain7);
    sliceVec.emplace_back(sliceDomain8);
    sliceVec.emplace_back(sliceDomain9);
    sliceVec.emplace_back(sliceDomain10);
    std::sort(sliceVec.begin(), sliceVec.end(), [](const SliceDomain &first, const SliceDomain &second) {
        if (first.timestamp < second.timestamp) {
            return true;
        }
        return (first.timestamp == second.timestamp) && first.id < second.id;
    });
    SliceQuery sliceQuery = {3, 0, 23, 2};
    SliceCacheManager::Instance().UpdateSliceCache("3", sliceVec, sliceQuery);
}
}
#endif // PROFILER_SERVER_SLICEANALYZER_MOCK_DATA_H
