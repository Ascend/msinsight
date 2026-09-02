#include "RankLaneMergeCoordinator.h"

#include <algorithm>
#include "FileUtil.h"

namespace Dic::Module::Timeline {
RankLaneMergeCoordinator &RankLaneMergeCoordinator::Instance() {
    static RankLaneMergeCoordinator instance;
    return instance;
}

std::string RankLaneMergeCoordinator::NormalizeSourceFileId(const std::string &fileId) {
    if (fileId.empty()) {
        return fileId;
    }
    std::string normalized = FileUtil::GetRealPath(fileId);
    if (normalized.empty()) {
        normalized = FileUtil::IsAbsolutePath(fileId) ? fileId : FileUtil::SplicePath(FileUtil::GetCurrPath(), fileId);
    }
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

bool RankLaneMergeCoordinator::IsReady(const RankStatus &rank) {
    return !rank.sources.empty() && std::all_of(rank.sources.begin(), rank.sources.end(), [](const auto &item) {
        return item.second.state != SourceState::AWAITING_TERMINAL;
    });
}

void RankLaneMergeCoordinator::RegisterSource(const std::string &rankId, const std::string &fileId) {
    if (rankId.empty() || fileId.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto &rank = ranks_[rankId];
    auto [source, inserted] = rank.sources.try_emplace(NormalizeSourceFileId(fileId));
    if (inserted) {
        source->second.originalFileId = fileId;
        rank.finalizing = false;
        rank.eventEmitted = false;
    }
}

void RankLaneMergeCoordinator::MarkSourceSucceeded(const std::string &rankId, const std::string &fileId) {
    if (rankId.empty() || fileId.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto &source = ranks_[rankId].sources[NormalizeSourceFileId(fileId)];
    if (source.state == SourceState::AWAITING_TERMINAL) {
        source.state = SourceState::PARSE_SUCCEEDED;
        source.error.clear();
    }
}

void RankLaneMergeCoordinator::MarkSourceFailed(
    const std::string &rankId, const std::string &fileId, const std::string &error) {
    if (rankId.empty() || fileId.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto &source = ranks_[rankId].sources[NormalizeSourceFileId(fileId)];
    if (source.state == SourceState::AWAITING_TERMINAL) {
        source.state = SourceState::PARSE_FAILED;
        source.error = error;
    }
}

void RankLaneMergeCoordinator::RemoveSource(const std::string &rankId, const std::string &fileId) {
    if (rankId.empty() || fileId.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto rank = ranks_.find(rankId);
    if (rank == ranks_.end()) {
        return;
    }
    rank->second.sources.erase(NormalizeSourceFileId(fileId));
    if (rank->second.sources.empty()) {
        ranks_.erase(rank);
        return;
    }
    rank->second.finalizing = false;
    rank->second.eventEmitted = false;
}

bool RankLaneMergeCoordinator::IsRankReady(const std::string &rankId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto rank = ranks_.find(rankId);
    return rank != ranks_.end() && IsReady(rank->second);
}

std::vector<std::string> RankLaneMergeCoordinator::GetSuccessfulSources(const std::string &rankId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> sources;
    auto rank = ranks_.find(rankId);
    if (rank == ranks_.end()) {
        return sources;
    }
    for (const auto &[normalizedFileId, status] : rank->second.sources) {
        (void)normalizedFileId;
        if (status.state == SourceState::PARSE_SUCCEEDED) {
            sources.emplace_back(status.originalFileId);
        }
    }
    return sources;
}

size_t RankLaneMergeCoordinator::GetRegisteredSourceCount(const std::string &rankId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto rank = ranks_.find(rankId);
    return rank == ranks_.end() ? 0 : rank->second.sources.size();
}

std::string RankLaneMergeCoordinator::GetRepresentativeSource(const std::string &rankId) const {
    auto sources = GetSuccessfulSources(rankId);
    return sources.empty() ? "" : sources.front();
}

bool RankLaneMergeCoordinator::TryMarkRankEventEmitted(const std::string &rankId) {
    if (!TryStartRankFinalization(rankId)) {
        return false;
    }
    MarkRankEventEmitted(rankId);
    return true;
}

bool RankLaneMergeCoordinator::TryStartRankFinalization(const std::string &rankId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto rank = ranks_.find(rankId);
    if (rank == ranks_.end() || !IsReady(rank->second) || rank->second.finalizing || rank->second.eventEmitted) {
        return false;
    }
    const bool hasSuccess = std::any_of(rank->second.sources.begin(), rank->second.sources.end(),
        [](const auto &item) { return item.second.state == SourceState::PARSE_SUCCEEDED; });
    if (!hasSuccess) {
        return false;
    }
    rank->second.finalizing = true;
    return true;
}

void RankLaneMergeCoordinator::MarkRankFinalizationFailed(const std::string &rankId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto rank = ranks_.find(rankId);
    if (rank != ranks_.end()) {
        rank->second.finalizing = false;
    }
}

void RankLaneMergeCoordinator::MarkRankEventEmitted(const std::string &rankId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto rank = ranks_.find(rankId);
    if (rank != ranks_.end() && rank->second.finalizing) {
        rank->second.eventEmitted = true;
        rank->second.finalizing = false;
    }
}

void RankLaneMergeCoordinator::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    ranks_.clear();
}
}
