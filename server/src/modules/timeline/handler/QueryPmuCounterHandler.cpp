/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "QueryPmuCounterHandler.h"
#include "DataBaseManager.h"
#include "PmuCounterRepository.h"
#include "PmuCounterCalculator.h"
#include "RankLaneMergeCoordinator.h"
#include "TraceTime.h"
#include <charconv>
#include <map>
#include <mutex>
#include <stdexcept>

namespace Dic::Module::Timeline {
namespace {
PmuCounterCapabilities GetPmuCapabilities(PmuCounterRepository &repository, const std::string &dbPath) {
    // Checkout shared_ptrs expire when returned to the pool. The pool token instead tracks the imported source lifetime.
    static std::mutex capabilityMutex;
    static std::map<std::weak_ptr<void>, PmuCounterCapabilities, std::owner_less<std::weak_ptr<void>>> capabilities;
    auto token = DataBaseManager::Instance().GetTraceDatabasePoolToken(dbPath);
    std::lock_guard<std::mutex> lock(capabilityMutex);
    for (auto entry = capabilities.begin(); entry != capabilities.end();) {
        entry = entry->first.expired() ? capabilities.erase(entry) : std::next(entry);
    }
    if (token.expired()) {
        return repository.DetectCapabilities();
    }
    auto cached = capabilities.find(token);
    if (cached != capabilities.end()) {
        return cached->second;
    }
    return capabilities.emplace(token, repository.DetectCapabilities()).first->second;
}

bool IsPmuSourceAssociated(const Protocol::PmuCounterParams &params) {
    auto &manager = DataBaseManager::Instance();
    const auto requested = RankLaneMergeCoordinator::NormalizeSourceFileId(params.dbPath);
    const auto sources = manager.GetSourceFileIdsByRankId(params.rankId);
    for (const auto &source : sources) {
        if (RankLaneMergeCoordinator::NormalizeSourceFileId(source) == requested) {
            return true;
        }
    }
    return false;
}
}

bool QueryPmuCounterHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    const auto &request = dynamic_cast<const Protocol::PmuCounterRequest &>(*requestPtr);
    auto response = std::make_unique<Protocol::PmuCounterResponse>();
    SetBaseResponse(request, *response);
    const auto origin = TraceTime::Instance().GetStartTime();
    std::string error;
    if (!request.params.CheckParams(origin, error) || !IsPmuSourceAssociated(request.params)) {
        Server::ServerLog::Warn("Invalid PMU counter request: ", error);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(response), false);
        return false;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(request.params.dbPath);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(response), false);
        return false;
    }
    int64_t counterId = 0;
    std::from_chars(
        request.params.counterId.data(), request.params.counterId.data() + request.params.counterId.size(), counterId);
    try {
        PmuCounterRepository repository(*database);
        const auto capabilities = GetPmuCapabilities(repository, request.params.dbPath);
        response->body.counterId = request.params.counterId;
        if (!capabilities.available) {
            response->body.status = "unsupported";
            SendResponse(std::move(response), true);
            return true;
        }
        std::optional<PmuCounterInfo> counter;
        if (!repository.GetCounter(counterId, counter, capabilities)) {
            throw std::runtime_error("Failed to read PMU counter metadata");
        }
        if (!counter.has_value()) {
            SetTimelineError(ErrorCode::PARAMS_ERROR);
            SendResponse(std::move(response), false);
            return false;
        }
        PmuCounterQuery query{request.params.startTime, request.params.endTime, origin, request.params.targetBuckets};
        PmuCounterCalculator calculator(*counter, capabilities, query);
        if (capabilities.hasRawValues &&
            !repository.ReadWindowWithNeighbors(counterId, origin + query.startTime, origin + query.endTime,
                capabilities, [&calculator](const PmuCounterSample &sample) { calculator.Consume(sample); })) {
            throw std::runtime_error("Failed to read PMU counter samples");
        }
        response->body = calculator.Finish();
        SendResponse(std::move(response), true);
        return true;
    } catch (const std::exception &exception) {
        Server::ServerLog::Error("PMU counter query failed: ", exception.what());
        SetTimelineError(ErrorCode::QUERY_PMU_COUNTER_FAILED);
        SendResponse(std::move(response), false);
        return false;
    }
}
}
