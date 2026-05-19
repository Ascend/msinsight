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

#ifndef PROFILER_SERVER_RANK_OFFSET_CALCULATOR_H
#define PROFILER_SERVER_RANK_OFFSET_CALCULATOR_H

#include <cstdint>
#include <set>
#include <string>
#include "TimelineProtocolResponse.h"

namespace Dic::Module::Timeline {

enum class RankOffsetSide {
    DEVICE,
    HOST,
    UNSUPPORTED,
};

enum class RankOffsetAlignType {
    LEFT,
    RIGHT,
    UNSUPPORTED,
};

class RankOffsetCalculator {
  public:
    static RankOffsetSide GetSide(const std::string &metaType) {
        static const std::set<std::string> deviceMetaTypes = {
            "Ascend Hardware",
            "HCCL",
            "OVERLAP_ANALYSIS",
        };
        static const std::set<std::string> hostMetaTypes = {
            "CANN_API",
            "PYTORCH_API",
            "OSRT_API",
            "MSTX_EVENTS",
            "TEXT",
        };
        if (deviceMetaTypes.count(metaType) != 0) {
            return RankOffsetSide::DEVICE;
        }
        if (hostMetaTypes.count(metaType) != 0) {
            return RankOffsetSide::HOST;
        }
        return RankOffsetSide::UNSUPPORTED;
    }

    static bool IsSameSide(const std::string &metaType, RankOffsetSide side) {
        return side != RankOffsetSide::UNSUPPORTED && GetSide(metaType) == side;
    }

    static RankOffsetAlignType ParseAlignType(const std::string &type) {
        if (type == "LEFT") {
            return RankOffsetAlignType::LEFT;
        }
        if (type == "RIGHT") {
            return RankOffsetAlignType::RIGHT;
        }
        return RankOffsetAlignType::UNSUPPORTED;
    }

    static int64_t AnchorTime(const Dic::Protocol::SimpleSlice &slice, RankOffsetAlignType type) {
        int64_t startTime = static_cast<int64_t>(slice.timestamp);
        if (type == RankOffsetAlignType::RIGHT) {
            return startTime + static_cast<int64_t>(slice.duration);
        }
        return startTime;
    }

    static int64_t CalculateOffset(const Dic::Protocol::SimpleSlice &baseSlice,
        const Dic::Protocol::SimpleSlice &targetSlice, RankOffsetAlignType type) {
        return AnchorTime(targetSlice, type) - AnchorTime(baseSlice, type);
    }
};
} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_RANK_OFFSET_CALCULATOR_H
