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

#include "RankMetadataMergePolicy.h"

#include "DomainObject.h"
#include "FullDbEnumUtil.h"

namespace Dic::Module::Timeline {
bool RankMetadataMergePolicy::ShouldInclude(const std::string &metaType, bool representative) {
    if (representative) {
        return true;
    }
    const auto overlap = Dic::Protocol::ENUM_TO_STR(PROCESS_TYPE::OVERLAP_ANALYSIS).value_or("OVERLAP_ANALYSIS");
    return metaType != overlap;
}
}
