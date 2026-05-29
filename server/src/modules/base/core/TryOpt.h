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

#ifndef PROFILER_SERVER_TRYOPT_H
#define PROFILER_SERVER_TRYOPT_H
#include "ServerLog.h"
namespace Dic {
namespace Module {
template <typename T> bool TryOpt(const std::shared_ptr<T> &ptr, const std::string &msg) {
    if (!ptr) {
        Dic::Server::ServerLog::Warn(msg);
        return false;
    }
    return true;
}

template <typename T> bool TryOpt(const std::unique_ptr<T> &ptr, const std::string &msg) {
    if (!ptr) {
        Dic::Server::ServerLog::Warn(msg);
        return false;
    }
    return true;
}

} // namespace Module
} // namespace Dic
#endif // PROFILER_SERVER_TRYOPT_H
