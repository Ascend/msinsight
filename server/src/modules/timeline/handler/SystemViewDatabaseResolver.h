#ifndef PROFILER_SERVER_SYSTEMVIEWDATABASERESOLVER_H
#define PROFILER_SERVER_SYSTEMVIEWDATABASERESOLVER_H

#include <memory>

#include "DataBaseManager.h"
#include "RankOverlapStore.h"
#include "TimelineProtocolRequest.h"

namespace Dic::Module::Timeline {
inline std::shared_ptr<VirtualTraceDatabase> ResolveSystemViewDatabase(const Protocol::SystemViewParams &params) {
    auto &manager = DataBaseManager::Instance();
    if (params.layer == "Overlap Analysis") {
        const std::string derivedSource = RankOverlapStore::Instance().ResolveSource(params.rankId);
        if (!derivedSource.empty()) {
            auto database = manager.GetTraceDatabaseByFileId(derivedSource);
            if (database != nullptr) {
                return database;
            }
        }
    }
    if (!params.dbPath.empty()) {
        auto database = manager.GetTraceDatabaseByFileId(params.dbPath);
        if (database != nullptr) {
            return database;
        }
    }
    return manager.GetTraceDatabaseByRankId(params.rankId);
}
}

#endif
