/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#include "QueryFreeAttributionHandler.h"

#include <limits>
#include <set>
#include <type_traits>
#include "FreeAttributionService.h"
#include "TraceTime.h"

namespace Dic::Module::Timeline {
namespace {
bool ValidateParams(const FreeAttributionParams &params, bool details, std::string &error) {
    if (!params.rangeMode.empty() && params.rangeMode != "all" && params.rangeMode != "range" &&
        params.rangeMode != "empty") {
        error = "Invalid Free attribution range mode.";
        return false;
    }
    if (params.rankId.empty()) {
        error = "Free attribution rank id is empty.";
        return false;
    }
    if (params.startTime > params.endTime ||
        params.endTime > std::numeric_limits<uint64_t>::max() - TraceTime::Instance().GetStartTime()) {
        error = "Invalid Free attribution time range.";
        return false;
    }
    if (!Protocol::CheckUnsignPageValid(params.pageSize, params.current, error) || params.pageSize > 1000) {
        error = "Invalid Free attribution pagination.";
        return false;
    }
    if ((params.current - 1) > std::numeric_limits<uint64_t>::max() / params.pageSize) {
        error = "Free attribution page offset is too large.";
        return false;
    }
    static const std::set<std::string> summaryFields = {"apiName", "totalFreeDuration", "freeCount"};
    static const std::set<std::string> detailFields = {
        "apiName", "apiStartTime", "apiDuration", "freeStartTime", "freeDuration"};
    const auto &fields = details ? detailFields : summaryFields;
    if (fields.count(params.orderBy) == 0 || (params.order != "asc" && params.order != "desc")) {
        error = "Invalid Free attribution sort field or order.";
        return false;
    }
    if (details && params.groupId.empty()) {
        error = "Free attribution group id is empty.";
        return false;
    }
    return true;
}

template <typename REQUEST, typename RESPONSE>
bool HandleAttribution(std::unique_ptr<Protocol::Request> requestPtr, bool details) {
    auto response = std::make_unique<RESPONSE>();
    auto *request = dynamic_cast<REQUEST *>(requestPtr.get());
    if (request == nullptr) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(response), false, "Invalid Free attribution request type.");
        return false;
    }
    ModuleRequestHandler::SetBaseResponse(*request, *response);
    std::string error;
    if (!ValidateParams(request->params, details, error)) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(response), false, error);
        return false;
    }
    FreeAttributionService service;
    bool success;
    if constexpr (std::is_same_v<RESPONSE, Protocol::FreeAttributionSummaryResponse>) {
        success = service.QuerySummary(request->params, response->body, error);
    } else {
        success = service.QueryDetails(request->params, response->body, error);
    }
    if (!success) {
        SetTimelineError(ErrorCode::QUERY_FREE_ATTRIBUTION_FAILED);
        SendResponse(std::move(response), false, error);
        return false;
    }
    SendResponse(std::move(response), true);
    return true;
}
}

bool QueryFreeAttributionSummaryHandler::HandleRequest(std::unique_ptr<Protocol::Request> request) {
    return HandleAttribution<Protocol::FreeAttributionSummaryRequest, Protocol::FreeAttributionSummaryResponse>(
        std::move(request), false);
}

bool QueryFreeAttributionDetailsHandler::HandleRequest(std::unique_ptr<Protocol::Request> request) {
    return HandleAttribution<Protocol::FreeAttributionDetailsRequest, Protocol::FreeAttributionDetailsResponse>(
        std::move(request), true);
}
}
