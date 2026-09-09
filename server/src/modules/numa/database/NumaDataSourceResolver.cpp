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

#include "pch.h"

#include <vector>

#include "DataBaseManager.h"
#include "NumaDataSourceResolver.h"

namespace Dic::Module::Numa {
namespace {
const std::vector<std::string> NUMA_TABLES = {
    "NUMA_TITLES_NAMES",
    "NUMA_LEVELS_HIERARCHY_NAMES",
    "NUMA_METRICS",
    "NUMA_SCALING_VALUES",
};

bool SupportsNumaOverview(const std::shared_ptr<Database> &database) {
    return database != nullptr && database->IsOpen() && database->CheckTablesExist(NUMA_TABLES);
}

std::shared_ptr<Database> GetInsightDatabase(const NumaDataSourceContext &context) {
    auto &manager = Timeline::DataBaseManager::Instance();
    if (!context.rankId.empty()) {
        auto database = manager.GetTraceDatabaseByRankId(context.rankId);
        if (SupportsNumaOverview(database)) {
            return database;
        }
    }

    if (context.fileId.empty()) {
        return nullptr;
    }
    auto database = manager.GetTraceDatabaseByFileId(context.fileId);
    return SupportsNumaOverview(database) ? database : nullptr;
}
} // namespace

std::shared_ptr<Database> NumaDataSourceResolver::Resolve(const NumaDataSourceContext &context) const {
    return GetInsightDatabase(context);
}

} // namespace Dic::Module::Numa
