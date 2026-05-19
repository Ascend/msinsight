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

#ifndef PROFILER_SERVER_SEARCH_ALL_SLICES_HANDLER_H
#define PROFILER_SERVER_SEARCH_ALL_SLICES_HANDLER_H

#include "TimelineRequestHandler.h"

// clang-format off
namespace Dic {
namespace Module {
namespace Timeline {

// 前向声明
class VirtualTraceDatabase;
struct LightSliceCache;
class SearchSliceCacheManager;
class CacheGuard;

class SearchAllSlicesHandler : public TimelineRequestHandler {
public:
    SearchAllSlicesHandler()
    {
        command = Protocol::REQ_RES_SEARCH_ALL_SLICES;
    };
    ~SearchAllSlicesHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

private:
    std::vector<TrackQuery> BuildTrackQueryVec(const Protocol::SearchAllSliceParams& params,
        const std::string& dbPath, uint64_t minTimestamp);
    bool HandleWithLockRange(VirtualTraceDatabase* database, const Protocol::SearchAllSliceParams& params,
        Protocol::SearchAllSlicesBody& body, uint64_t minTimestamp, const std::vector<TrackQuery>& trackQueryVec);
    bool HandleWithSoACache(VirtualTraceDatabase* database, const Protocol::SearchAllSliceParams& params,
        Protocol::SearchAllSlicesBody& body, uint64_t minTimestamp);
    bool LoadOrRefreshCache(CacheGuard& guard, SearchSliceCacheManager& cacheManager,
        const std::string& cacheKey, VirtualTraceDatabase* database,
        const Protocol::SearchAllSliceParams& params, uint64_t minTimestamp);
    bool FallbackToSqlQuery(VirtualTraceDatabase* database, const Protocol::SearchAllSliceParams& params,
        Protocol::SearchAllSlicesBody& body, uint64_t minTimestamp);
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
// clang-format on

#endif // PROFILER_SERVER_SEARCH_ALL_SLICES_HANDLER_H
