/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_MSPROFFTRACEIMPORTSERVICE_H
#define PROFILER_SERVER_MSPROFFTRACEIMPORTSERVICE_H

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "MsprofFtraceTypes.h"

namespace Dic::Module::Global {
struct BaselineInfo;
}
namespace Dic::Module::FullDb {

struct MsprofFtraceResource {
    std::string sourceFileId;
    std::string fileId;
    std::string rankId;
    std::string projectName;
    std::string host;
    std::string cluster;
    std::string cachePath;
    std::string error;
    std::vector<std::string> capabilities;
    MsprofFtraceInspection inspection;
    bool ready = false;
};

class MsprofFtraceImportService {
  public:
    static MsprofFtraceImportService &Instance();
    static std::string ResourceId(const std::string &sourceFileId);

    std::optional<MsprofFtraceResource> Discover(const std::string &sourceFileId, const std::string &host = "",
        const std::string &cluster = "", const std::string &projectName = "");
    std::optional<MsprofFtraceResource> GetResource(const std::string &resourceId) const;
    bool IsResource(const std::string &resourceId) const;
    std::vector<std::string> GetCapabilities(const std::string &resourceId) const;
    bool HasSource(const std::string &sourceFileId) const;

    // Synchronous core used by both normal import and focused integration tests.
    bool Import(const std::string &sourceFileId, std::string &error, bool notify = false);
    void Schedule(const std::string &sourceFileId);
    // Non-blocking preflight only; CreateBaseline rechecks under the same build lock.
    bool CheckBaselineReady(const std::string &resourceId, std::string &error) const;
    // Current-project baselines reuse the visible source; cross-project baselines get an independent alias.
    bool CreateBaseline(const std::string &resourceId, Global::BaselineInfo &info, bool reuseSource = false);
    void NotifyReady(const std::string &resourceId);
    // Project switches may keep completed files without retaining connections or track registrations.
    bool Remove(const std::string &resourceOrRankId, bool retainCache = false);
    void OnDatabaseReleased(const std::string &fileId);
    void Reset(bool retainCaches = false);

  private:
    struct Entry {
        MsprofFtraceResource resource;
        std::mutex buildMutex;
        std::atomic<bool> cancelled{false};
        std::atomic<bool> scheduled{false};
        std::string fingerprint;
        std::set<std::string> cachePins;
    };
    MsprofFtraceImportService();
    ~MsprofFtraceImportService();
    MsprofFtraceImportService(const MsprofFtraceImportService &) = delete;
    MsprofFtraceImportService &operator=(const MsprofFtraceImportService &) = delete;

    std::shared_ptr<Entry> FindEntry(const std::string &id) const;
    bool Build(const std::shared_ptr<Entry> &entry, std::string &error, bool notify);
    void PublishCompletion(const MsprofFtraceResource &resource, bool success, const std::string &error);
    void RetainCache(const MsprofFtraceResource &resource);
    std::string TakeRetainedCache(const std::string &sourceFileId);
    void RemoveUnusedCache(const std::string &path);
    struct RetainedCache {
        std::string path;
        uint64_t lastUsed = 0;
    };
    struct Worker;
    std::shared_ptr<Worker> worker_;
    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<Entry>> resources_;
    std::map<std::string, RetainedCache> retainedCaches_;
    uint64_t cacheSequence_ = 0;
    std::atomic<uint64_t> generation_{0};
};
}
#endif
