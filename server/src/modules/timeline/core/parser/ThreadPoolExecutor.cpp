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
#include "pch.h"
#include "ClusterParseThreadPoolExecutor.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

ClusterParseThreadPoolExecutor &ClusterParseThreadPoolExecutor::Instance() {
    static ClusterParseThreadPoolExecutor instance;
    return instance;
}

ClusterParseThreadPoolExecutor::ClusterParseThreadPoolExecutor() {
    singleThreadPool = std::make_shared<ThreadPool>(1);
    ServerLog::Info("single thread pool init success");
}

ClusterParseThreadPoolExecutor::~ClusterParseThreadPoolExecutor() { singleThreadPool->ShutDown(); }

std::shared_ptr<ThreadPool> ClusterParseThreadPoolExecutor::GetThreadPool() { return singleThreadPool; }

} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
