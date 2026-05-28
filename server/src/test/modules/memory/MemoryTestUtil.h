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
#include "OperatorMemoryService.h"

namespace Dic::Module::Memory {
namespace OperatorMemoryTestUtil {
static bool IsOperatorsSorted(const std::vector<MemoryOperator> &operators, std::string_view orderBy, bool desc) {
    auto comparator = OperatorMemoryService::GetComparatorByColumn(orderBy, desc);
    return std::is_sorted(operators.begin(), operators.end(), comparator);
}

static bool IsOperatorsNameAllContains(const std::vector<MemoryOperator> &operators, const std::string &subStr) {
    return std::all_of(operators.begin(), operators.end(),
        [subStr](const MemoryOperator &op) { return StringUtil::ContainsIgnoreCase(op.name, subStr); });
}
}
}
