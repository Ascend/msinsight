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

#include "TopNAdviceMaintainer.h"

namespace Dic::Module::Summary {

double GetTotalSynchronizeTime(const AdviceInfoForSlowRank &info) {
    double total = 0.0;
    for (const auto &[pg, t] : info.synchronizeTime) {
        (void)(pg);
        total += t;
    }
    return total;
}

void TopNAdviceMaintainer::Insert(const AdviceInfoForSlowRank &info) {
    queue.push(info);
    if (queue.size() > length) {
        queue.pop();
    }
}

std::vector<AdviceInfoForSlowRank> TopNAdviceMaintainer::GetTopNSlowest(uint32_t topN) const {
    // 拷贝堆内容
    std::vector<AdviceInfoForSlowRank> all;
    auto tempQueue = queue;
    while (!tempQueue.empty()) {
        all.push_back(tempQueue.top());
        tempQueue.pop();
    }
    std::reverse(all.begin(), all.end());

    // 截取前 topN
    if (static_cast<size_t>(topN) < all.size()) {
        all.resize(topN);
    }
    return all;
}

bool TopNAdviceMaintainer::IsEmpty() const { return queue.empty(); }
}
