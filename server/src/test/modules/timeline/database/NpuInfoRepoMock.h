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

#ifndef PROFILER_SERVER_NPUINFOREPOMOCK_H
#define PROFILER_SERVER_NPUINFOREPOMOCK_H
#include "NpuInfoRepo.h"
#include "TraceDatabaseHelper.h"

class NpuInfoRepoMock : public Dic::Protocol::NpuInfoRepo {
  public:
    std::vector<uint64_t> QueryDeviceIdByFileId(const std::string &fileId) override {
        std::vector<uint64_t> res;
        res.push_back(0);
        return res;
    }
};
inline void MockNpuInfoRepoFunc() {
    std::unique_ptr<Dic::Protocol::NpuInfoRepo> tPtr = std::make_unique<NpuInfoRepoMock>();
    Dic::Protocol::TraceDatabaseHelper::SetNpuInfoRepo(std::move(tPtr));
}

inline void RestoreRepoFunc() {
    std::unique_ptr<Dic::Protocol::NpuInfoRepo> tPtr = std::make_unique<Dic::Protocol::NpuInfoRepo>();
    Dic::Protocol::TraceDatabaseHelper::SetNpuInfoRepo(std::move(tPtr));
}

#endif // PROFILER_SERVER_NPUINFOREPOMOCK_H
