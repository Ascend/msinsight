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

#ifndef PROFILER_SERVER_DBPLATFORMDATABASE_H
#define PROFILER_SERVER_DBPLATFORMDATABASE_H

#include "VirtualPlatformDataBase.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"
#include <map>
#include <vector>

namespace Dic {
namespace Module {
namespace FullDb {

struct LevelData {
    int64_t levelId = 0;
    int64_t title0Id = 0;
    int64_t title1Id = 0;
    int64_t title2Id = 0;
};

struct TitleData {
    int64_t id = 0;
    std::string name;
    std::string description;
    std::string measurementUnit;
    int64_t summaryFlag = 0;
};

using LevelDataMap = std::map<int64_t, LevelData>;
using TitleDataMap = std::map<int64_t, TitleData>;
using ScalingValueMap = std::map<int64_t, double>;

struct PlatformMetric {
    int64_t ts = 0;
    int64_t levelId = 0;
    double value = 0;
    int64_t title0Id = 0;
};

struct PlatformCounterData {
    uint64_t timestamp = 0;
    double value = 0;
};

class DbPlatformDataBase : public Platform::VirtualPlatformDataBase {
  public:
    explicit DbPlatformDataBase(std::recursive_mutex &sqlMutex) : Platform::VirtualPlatformDataBase(sqlMutex) {};
    ~DbPlatformDataBase() override = default;

    bool QueryLevelData(LevelDataMap &levels);
    bool QueryTitleData(TitleDataMap &titles);
    bool QueryPlatformMetrics(std::vector<PlatformMetric> &metrics);
    bool QueryPlatformCounterData(
        int64_t levelId, uint64_t startTime, uint64_t endTime, std::vector<PlatformCounterData> &dataList);
    bool QueryUnitCounter(Dic::Protocol::UnitCounterParams &params, uint64_t minTimestamp,
        std::vector<Dic::Protocol::UnitCounterData> &dataList);
    bool QueryMeasurementUnit(int64_t levelId, std::string &measurementUnit);
    bool QueryScalingValuesData(ScalingValueMap &scalingValueMap);

    static void Reset();
};

}
}
}

#endif // PROFILER_SERVER_DBPLATFORMDATABASE_H
