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
#include "RequestFilter.h"

namespace Dic::Server {
void RequestFilter::SetRequestId(const std::string &key, unsigned int id) {
    SpinLockGuard lock(mutex);
    auto it = filterMap.find(key);
    if (it == filterMap.end() || id > it->second) {
        filterMap[key] = id;
    }
}

bool RequestFilter::IsNeedFilter(const std::string &key, unsigned int id) {
    SpinLockGuard lock(mutex);
    auto it = filterMap.find(key);
    if (it == filterMap.end()) {
        return false;
    }
    if (it->second > id) {
        return true;
    }
    return false;
}

void RequestFilter::ClearKey(const std::string &key, unsigned int id) {
    SpinLockGuard lock(mutex);
    auto it = filterMap.find(key);
    if (it != filterMap.end() && it->second == id) {
        filterMap.erase(key);
    }
}

RequestFilter &RequestFilter::Instance() {
    static RequestFilter requestFilter;
    return requestFilter;
}
}
