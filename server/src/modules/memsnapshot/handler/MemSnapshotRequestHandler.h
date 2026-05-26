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

#ifndef PROFILER_SERVER_MEM_SNAPSHOT_REQUEST_HANDLER_H
#define PROFILER_SERVER_MEM_SNAPSHOT_REQUEST_HANDLER_H

#include "DataBaseManager.h"
#include "pch.h"
#include "ModuleRequestHandler.h"
#include "ProtocolDefs.h"
#include "NumberUtil.h"
#include "MemSnapshotProtocolRequest.h"
#include "MemSnapshotProtocolResponse.h"
#include "MemSnapshotDatabase.h"

namespace Dic::Module::MemSnapshot {
class MemSnapshotRequestHandler : public ModuleRequestHandler {
  public:
    MemSnapshotRequestHandler() {
        moduleName = MODULE_MEM_SCOPE; // Snapshot与MemScope为同一模块下的两种类型，为了方便处理，这里统一为MemScope
        async = false;
    }

    ~MemSnapshotRequestHandler() override = default;

  protected:
    inline static const std::string REQUEST_ERROR_UNKNOWN = "An unknown exception occurred while querying data. "
                                                            "Please check whether your data contains anomalies or "
                                                            "review the logs for more information.";

    static inline std::shared_ptr<FullDb::MemSnapshotDatabase> GetMemSnapshotDatabaseByRequest(
        const Protocol::Request &request) {
        return Timeline::DataBaseManager::Instance().GetMemSnapshotDatabase(
            request.fileId.empty() ? request.projectName : request.fileId);
    }
};
} // namespace Dic::Module::MemSnapshot
#endif // PROFILER_SERVER_MEM_SNAPSHOT_REQUEST_HANDLER_H
