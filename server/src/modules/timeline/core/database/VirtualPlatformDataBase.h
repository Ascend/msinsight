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

#ifndef PROFILER_SERVER_VIRTUALPLATFORMDATABASE_H
#define PROFILER_SERVER_VIRTUALPLATFORMDATABASE_H

#include <string>
#include <vector>
#include "Database.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"

namespace Dic {
namespace Module {
namespace Platform {

class VirtualPlatformDataBase : public Database {
  public:
    explicit VirtualPlatformDataBase(std::recursive_mutex &sqlMutex) : Database(sqlMutex) {};
    ~VirtualPlatformDataBase() override = default;

    virtual bool QueryUnitCounter(Dic::Protocol::UnitCounterParams &params, uint64_t minTimestamp,
        std::vector<Dic::Protocol::UnitCounterData> &dataList) = 0;
};

}
}
}

#endif // PROFILER_SERVER_VIRTUALPLATFORMDATABASE_H
