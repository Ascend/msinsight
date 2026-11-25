/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
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
