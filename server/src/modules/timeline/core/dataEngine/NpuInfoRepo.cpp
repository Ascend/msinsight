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
#include "NpuInfoRepo.h"

namespace Dic::Module::Timeline {
std::vector<uint64_t> NpuInfoRepo::QueryDeviceIdByFileId(const std::string &fileId) {
    std::vector<NpuInfoPo> npuInfoPos =
        npuInfoTable->Select(NpuInfoColumn::ID).GroupBy(NpuInfoColumn::ID).ExcuteQuery(fileId);
    std::vector<uint64_t> deviceIdList;
    for (const auto &item : npuInfoPos) {
        deviceIdList.push_back(item.id);
    }
    return deviceIdList;
}

void NpuInfoRepo::SetNpuInfoTable(std::unique_ptr<NpuInfoTable> npuInfoTablePtr) {
    if (npuInfoTablePtr != nullptr) {
        npuInfoTable = std::move(npuInfoTablePtr);
    }
}
}
