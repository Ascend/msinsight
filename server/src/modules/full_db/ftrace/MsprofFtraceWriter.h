/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_MSPROFFTRACEWRITER_H
#define PROFILER_SERVER_MSPROFFTRACEWRITER_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "EventDef.h"
#include "MsprofFtraceBuilder.h"

namespace Dic::Module::Timeline {
class TextTraceDatabase;
}

namespace Dic::Module::FullDb {

// Owns only writes to a private, already-open TEXT cache. It never registers or publishes resources.
class MsprofFtraceWriter final : public MsprofFtraceSink {
  public:
    explicit MsprofFtraceWriter(std::shared_ptr<Timeline::TextTraceDatabase> database);
    bool Begin(std::string &error);
    bool AddSlice(const MsprofFtraceSlice &slice, std::string &error) override;
    bool Finish(std::string &error);

  private:
    bool Flush(std::string &error);
    bool EnsureProcess(const std::string &process, std::string &error);
    uint64_t EnsureTrack(const MsprofFtraceSlice &slice, std::string &error);
    std::shared_ptr<Timeline::TextTraceDatabase> database;
    std::map<std::pair<std::string, std::string>, uint64_t> trackIds;
    std::set<std::string> processIds;
    std::vector<Timeline::Trace::Slice> slices;
    uint64_t nextTrackId = 0;
    bool begun = false;
    bool finished = false;
};

}
#endif
