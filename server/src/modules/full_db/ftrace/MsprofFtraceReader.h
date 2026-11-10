/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_MSPROFFTRACEREADER_H
#define PROFILER_SERVER_MSPROFFTRACEREADER_H

#include <functional>
#include "MsprofFtraceTypes.h"

namespace Dic::Module::FullDb {

class MsprofFtraceReader {
  public:
    using Consumer = std::function<bool(const MsprofFtraceEvent &)>;
    using Cancelled = std::function<bool()>;

    static MsprofFtraceInspection Inspect(const std::string &path);

    // Only supported scheduling/interrupt tracepoints are decoded. Other source rows remain untouched.
    // All callbacks run synchronously. Returning false stops the read without publishing a partial cache.
    static bool Read(
        const std::string &path, const Consumer &consumer, std::string &error, const Cancelled &cancelled = {});
};

}
#endif
