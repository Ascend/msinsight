/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_MSPROFFTRACECACHESCHEMA_H
#define PROFILER_SERVER_MSPROFFTRACECACHESCHEMA_H
namespace Dic::Module::FullDb {
// An empty table identifies a private adapter cache; only the final row marks it reusable.
inline constexpr const char *MSPROF_FTRACE_CACHE_SCHEMA =
    "CREATE TABLE IF NOT EXISTS msprof_ftrace_cache(fingerprint TEXT,version TEXT,capabilities TEXT)";
}
#endif
