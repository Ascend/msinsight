/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#include "pch.h"

#include "NumaDataSourceResolver.h"
#include "NumaMetricRepository.h"
#include "NumaOverviewAssembler.h"
#include "NumaProtocol.h"
#include "QueryNumaOverviewHandler.h"
#include "ServerLog.h"

namespace Dic::Module::Numa {
namespace {
constexpr int NUMA_ERROR_INVALID_RANGE = 4003;
constexpr int NUMA_ERROR_DATABASE_NOT_FOUND = 4004;
constexpr int NUMA_ERROR_QUERY_FAILED = 5001;
} // namespace

bool QueryNumaOverviewHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<Protocol::NumaOverviewRequest &>(*requestPtr);
    auto response = std::make_unique<Protocol::NumaOverviewResponse>();
    SetBaseResponse(request, *response);

    if (request.startTime > request.endTime) {
        Server::ServerLog::Error("NUMA overview start time is greater than end time");
        SendResponse(
            std::move(response), false, "NUMA overview startTime must not exceed endTime.", NUMA_ERROR_INVALID_RANGE);
        return false;
    }

    const NumaDataSourceContext context{request.rankId, request.fileId};
    const auto database = NumaDataSourceResolver().Resolve(context);
    if (database == nullptr) {
        Server::ServerLog::Error("NUMA overview failed to resolve data source. project: ", request.projectName,
            ", fileId: ", request.fileId);
        SendResponse(std::move(response), false, "No NUMA data source containing the required metric tables was found.",
            NUMA_ERROR_DATABASE_NOT_FOUND);
        return false;
    }

    NumaMetricRepository repository(database);
    std::vector<NumaMetricRecord> records;
    if (!repository.QueryMetricRecords(request.startTime, request.endTime, records) || records.empty()) {
        Server::ServerLog::Error("NUMA overview query returned no data");
        SendResponse(std::move(response), false, "No NUMA metrics were found in the selected data source.",
            NUMA_ERROR_QUERY_FAILED);
        return false;
    }

    NumaOverviewData overview = NumaOverviewAssembler().Build(records);
    response->rangeStart = overview.rangeStart;
    response->rangeEnd = overview.rangeEnd;
    response->totalMetrics = std::move(overview.totalMetrics);
    response->sockets = std::move(overview.sockets);
    response->connections = std::move(overview.connections);

    SendResponse(std::move(response), true);
    return true;
}
} // namespace Dic::Module::Numa
