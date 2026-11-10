/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_MSPROFFTRACEBUILDER_H
#define PROFILER_SERVER_MSPROFFTRACEBUILDER_H

#include <map>
#include <memory>
#include <optional>
#include <string>
#include "MsprofFtraceTypes.h"

namespace Dic::Module::FullDb {

struct MsprofFtraceSlice {
    std::string process;
    std::string threadKey;
    std::string threadName;
    std::string name;
    std::string args;
    uint64_t timestampNs = 0;
    uint64_t durationNs = 0;
};

class MsprofFtraceSink {
  public:
    virtual ~MsprofFtraceSink() = default;
    virtual bool AddSlice(const MsprofFtraceSlice &slice, std::string &error) = 0;
};

struct MsprofFtraceBuildSummary {
    uint64_t eventCount = 0;
    uint64_t trustedEventCount = 0;
    uint64_t failedEventCount = 0;
    uint64_t sliceCount = 0;
    std::optional<uint64_t> minTimestampNs;
    std::optional<uint64_t> maxTimestampNs;
    bool hasScheduling = false;
    bool hasWakeup = false;
    bool hasSoftirq = false;
    bool hasHardirq = false;
};

class MsprofFtraceBuilder {
  public:
    explicit MsprofFtraceBuilder(MsprofFtraceSink &sink, const std::map<uint32_t, uint32_t> &knownTgids = {});
    ~MsprofFtraceBuilder();
    MsprofFtraceBuilder(const MsprofFtraceBuilder &) = delete;
    MsprofFtraceBuilder &operator=(const MsprofFtraceBuilder &) = delete;

    bool Consume(const MsprofFtraceEvent &event, std::string &error);
    // An absent closing event is not replaced by the end of the profiling session.
    bool Finish(std::string &error);
    const MsprofFtraceBuildSummary &GetSummary() const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

}
#endif
