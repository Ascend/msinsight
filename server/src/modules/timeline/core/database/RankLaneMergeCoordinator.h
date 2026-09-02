#ifndef PROFILER_SERVER_RANKLANEMERGECOORDINATOR_H
#define PROFILER_SERVER_RANKLANEMERGECOORDINATOR_H

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Dic::Module::Timeline {
class RankLaneMergeCoordinator {
  public:
    static RankLaneMergeCoordinator &Instance();
    static std::string NormalizeSourceFileId(const std::string &fileId);

    void RegisterSource(const std::string &rankId, const std::string &fileId);
    void MarkSourceSucceeded(const std::string &rankId, const std::string &fileId);
    void MarkSourceFailed(const std::string &rankId, const std::string &fileId, const std::string &error);
    void RemoveSource(const std::string &rankId, const std::string &fileId);
    bool IsRankReady(const std::string &rankId) const;
    std::vector<std::string> GetSuccessfulSources(const std::string &rankId) const;
    size_t GetRegisteredSourceCount(const std::string &rankId) const;
    std::string GetRepresentativeSource(const std::string &rankId) const;
    bool TryStartRankFinalization(const std::string &rankId);
    void MarkRankFinalizationFailed(const std::string &rankId);
    void MarkRankEventEmitted(const std::string &rankId);
    bool TryMarkRankEventEmitted(const std::string &rankId);
    void Reset();

  private:
    enum class SourceState { AWAITING_TERMINAL, PARSE_SUCCEEDED, PARSE_FAILED };
    struct SourceStatus {
        SourceState state = SourceState::AWAITING_TERMINAL;
        std::string originalFileId;
        std::string error;
    };
    struct RankStatus {
        std::map<std::string, SourceStatus> sources;
        bool finalizing = false;
        bool eventEmitted = false;
    };

    RankLaneMergeCoordinator() = default;
    static bool IsReady(const RankStatus &rank);

    mutable std::mutex mutex_;
    std::map<std::string, RankStatus> ranks_;
};
}

#endif
