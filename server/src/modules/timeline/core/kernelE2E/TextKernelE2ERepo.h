/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_TEXTKERNELE2EREPO_H
#define PROFILER_SERVER_TEXTKERNELE2EREPO_H

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>
#include "DomainObject.h"
#include "KernelE2ERepoInterface.h"
#include "SqliteResultSet.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Timeline {

class TextKernelE2ERepo : public KernelE2ERepoInterface {
  public:
    TextKernelE2ERepo(std::shared_ptr<VirtualTraceDatabase> database, std::string fileId)
        : database_(std::move(database)), fileId_(std::move(fileId)) {}

    std::vector<KernelE2EEvent> QueryPythonApiEvents(const KernelE2EQuery &query) override;
    std::vector<KernelE2EEvent> QueryCannApiEvents(const KernelE2EQuery &query) override;
    std::vector<KernelE2EEvent> QueryHardwareTaskEvents(const KernelE2EQuery &query,
        const std::vector<KernelE2EEvent> &pythonEvents, const std::vector<KernelE2EEvent> &cannEvents) override;
    std::vector<KernelE2EFlow> QueryFlows(const KernelE2EQuery &query, const std::vector<KernelE2EEvent> &pythonEvents,
        const std::vector<KernelE2EEvent> &cannEvents, const std::vector<KernelE2EEvent> &hardwareTasks) override;

  private:
    struct TextFlowPoint {
        uint64_t id = 0;
        std::string flowId;
        std::string name;
        std::string cat;
        uint64_t trackId = 0;
        uint64_t timestamp = 0;
        std::string type;
    };

    std::vector<KernelE2EEvent> QuerySliceEvents(
        const KernelE2EQuery &query, const std::string &nameFilterSql, const std::string &orderBySql) const;
    KernelE2EEvent BuildEventFromResultSet(const std::unique_ptr<Dic::Module::SqliteResultSet> &resultSet) const;
    std::optional<KernelE2EEvent> QuerySliceEventById(uint64_t sliceId) const;
    std::vector<TextFlowPoint> QueryFlowPointsByCategories(const KernelE2EQuery &query) const;
    std::vector<TextFlowPoint> QueryFlowPointsByFlowIds(const std::vector<std::string> &flowIds) const;
    std::optional<KernelE2EEvent> MapFlowPointToEvent(const TextFlowPoint &flowPoint) const;
    std::vector<SliceDomain> QuerySlicesByTrackId(uint64_t trackId) const;
    static KernelE2EFlow BuildFlowFromPoints(const TextFlowPoint &fromPoint, const KernelE2EEvent &fromEvent,
        const TextFlowPoint &toPoint, const KernelE2EEvent &toEvent);
    static bool IsFlowStart(const std::string &type);
    static bool IsFlowEnd(const std::string &type);
    static void ClassifyPythonEvent(KernelE2EEvent &event);
    static void ClassifyCannEvent(KernelE2EEvent &event);
    static void ClassifyHardwareEvent(KernelE2EEvent &event);

    std::shared_ptr<VirtualTraceDatabase> database_;
    std::string fileId_;
    mutable std::unordered_map<uint64_t, std::vector<SliceDomain>> slicesByTrackId_;
    mutable std::unordered_map<uint64_t, KernelE2EEvent> eventsBySliceId_;
};

} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_TEXTKERNELE2EREPO_H
