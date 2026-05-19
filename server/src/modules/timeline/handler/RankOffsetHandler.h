/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can be obtained a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_RANK_OFFSET_HANDLER_H
#define PROFILER_SERVER_RANK_OFFSET_HANDLER_H

#include "TimelineRequestHandler.h"
#include "RankOffsetCalculator.h"

namespace Dic {
namespace Module {
namespace Timeline {

struct FailedRank;

class RankOffsetHandler : public TimelineRequestHandler {
  public:
    RankOffsetHandler() { command = Protocol::REQ_RES_RANK_OFFSET; };
    ~RankOffsetHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    /**
     * Resolve the base slice by querying the database, filtering by pid,
     * sorting by timestamp, and matching the user-selected slice via startTime/duration.
     * Returns the slice and its 0-based ordinal index.
     */
    bool ResolveBaseSlice(const RankOffsetParams &params, RankOffsetSide side, Protocol::SimpleSlice &outSlice,
        int64_t &outIndex, std::string &errorMsg);

    /**
     * Query same-name slices on a target rank, match by ordinal index,
     * and compute the offset. On failure, records the reason in failedRanks.
     */
    void ProcessTargetRank(const std::string &targetRankId, const std::string &sliceName, const std::string &metaType,
        RankOffsetSide side, RankOffsetAlignType alignType, const Protocol::SimpleSlice &baseSlice, int64_t baseIndex,
        uint64_t baseOffset, std::vector<RankOffsetItem> &successList, std::vector<FailedRank> &failedRanks);
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_RANK_OFFSET_HANDLER_H
