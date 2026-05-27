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

#ifndef PROFILER_SERVER_PARALLELSTRATEGYALGORITHMHELPER_H
#define PROFILER_SERVER_PARALLELSTRATEGYALGORITHMHELPER_H

#include <string>
#include <vector>
#include <cstdint>

namespace Dic::Module::Summary {
using allGroupsType = std::vector<std::vector<uint32_t>>;

class ParallelStrategyAlgorithmHelper {
  public:
    static allGroupsType GetAllGroupsRanksByToken(const std::vector<std::string> &token,
        const std::vector<uint32_t> &parallelSize, const std::vector<std::string> &order, uint32_t worldSize);

  private:
    static std::vector<bool> GetMask(const std::vector<std::string> &order, const std::vector<std::string> &token);
    static allGroupsType GenerateMaskedOrthogonalRankGroups(
        const std::vector<uint32_t> &parallelSize, const std::vector<bool> &mask, uint32_t wordSize);
    static std::vector<uint32_t> prefixProduct(const std::vector<uint32_t> &sizeList, uint32_t init);
    static uint32_t innerProduct(const std::vector<uint32_t> &x, const std::vector<uint32_t> &y);
    static std::vector<uint32_t> Decompose(uint32_t index, const std::vector<uint32_t> &shape);
};
}
#endif // PROFILER_SERVER_PARALLELSTRATEGYALGORITHMHELPER_H
