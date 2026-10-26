/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#include "FreeAttributionService.h"
#include "FreeAttributionRepository.h"

namespace Dic::Module::Timeline {
bool FreeAttributionService::QuerySummary(
    const FreeAttributionParams &params, FreeAttributionSummary &result, std::string &error) {
    if (params.rangeMode == "empty") {
        result = {};
        error.clear();
        return true;
    }
    std::vector<FreeAttributionRecord> records;
    if (!FreeAttributionRepository::Load(params, records, error)) {
        return false;
    }
    Summarize(records, params, result);
    return true;
}

bool FreeAttributionService::QueryDetails(
    const FreeAttributionParams &params, FreeAttributionDetails &result, std::string &error) {
    if (params.rangeMode == "empty") {
        result = {};
        error.clear();
        return true;
    }
    std::vector<FreeAttributionRecord> records;
    if (!FreeAttributionRepository::Load(params, records, error)) {
        return false;
    }
    SelectDetails(records, params, result);
    return true;
}
}
