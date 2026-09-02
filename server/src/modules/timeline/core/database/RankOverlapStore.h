#ifndef PROFILER_SERVER_RANKOVERLAPSTORE_H
#define PROFILER_SERVER_RANKOVERLAPSTORE_H

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "DbTraceDataBase.h"

struct sqlite3;

namespace Dic::Module::Timeline {
class RankOverlapStore {
  public:
    enum class PublishFailurePointForTesting { NONE, WRITE_DATABASE, CREATE_CONNECTION_POOL };

    static RankOverlapStore &Instance();

    std::string Publish(
        const std::string &rankId, const std::string &deviceId, const std::vector<FullDb::OVERLAP_INFO> &rows);
    std::string ResolveSource(const std::string &rankId) const;
    void Invalidate(const std::string &rankId);
    void Clear();
    void SetPublishFailurePointForTesting(PublishFailurePointForTesting failurePoint);

  private:
    struct Source {
        std::string uri;
        sqlite3 *keeper = nullptr;
    };

    RankOverlapStore() = default;
    static std::string CreateSourceUri(const std::string &rankId, uint64_t generation);
    static bool WriteDatabase(
        Source &source, const std::string &deviceId, const std::vector<FullDb::OVERLAP_INFO> &rows);
    static void RemoveSource(const std::string &rankId, Source &source);

    mutable std::mutex mutex_;
    std::map<std::string, Source> sources_;
    uint64_t generation_ = 0;
    PublishFailurePointForTesting failurePointForTesting_ = PublishFailurePointForTesting::NONE;
};
}

#endif
