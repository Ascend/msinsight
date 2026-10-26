/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#ifndef PROFILER_SERVER_FREEATTRIBUTIONREPOSITORY_H
#define PROFILER_SERVER_FREEATTRIBUTIONREPOSITORY_H

#include "FreeAttributionService.h"

namespace Dic::Module::Timeline {
class FreeAttributionRepository {
  public:
    static bool Load(
        const FreeAttributionParams &params, std::vector<FreeAttributionRecord> &records, std::string &error);
};
}
#endif
