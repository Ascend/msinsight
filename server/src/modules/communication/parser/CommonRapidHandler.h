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

#ifndef PROFILER_SERVER_COMMONRAPIDHANDLER_H
#define PROFILER_SERVER_COMMONRAPIDHANDLER_H

#include <unordered_map>
#include <string>
#include "ClusterDef.h"
#include "TextClusterDatabase.h"

namespace Dic {
namespace Module {
namespace Timeline {
class CommonRapidHandler {
  protected:
    bool isOldData = false;
    std::unordered_map<std::string, CommGroupParallelInfo> groupIdsMap;
    uint64_t groupIdIndex = 0;
    std::shared_ptr<TextClusterDatabase> database;
    std::string GenerateAndGetGroupInfoId(const std::string &rankSet, const std::string &groupIdHash);
    void InitGroupInfoMap();
    bool SaveGroupInfoMap();
};
}
}
}
#endif // PROFILER_SERVER_COMMONRAPIDHANDLER_H
