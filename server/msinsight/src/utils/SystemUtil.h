/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_SYSTEM_UTIL_H
#define PROFILER_SERVER_SYSTEM_UTIL_H

#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace Dic {
class SystemUtil {
  public:
    static unsigned int GetCpuCoreCount() {
#ifdef _WIN32
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwNumberOfProcessors;
#else
        return sysconf(_SC_NPROCESSORS_CONF);
#endif
    }

    static uint64_t GetAvailablePhysicalMemoryBytes() {
#ifdef _WIN32
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        if (GlobalMemoryStatusEx(&status)) {
            return static_cast<uint64_t>(status.ullAvailPhys);
        }
#elif defined(__APPLE__)
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        vm_statistics64_data_t vmStats;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmStats), &count) ==
            KERN_SUCCESS) {
            return static_cast<uint64_t>(vmStats.free_count + vmStats.inactive_count) *
                static_cast<uint64_t>(getpagesize());
        }
#else
        const long availablePages = sysconf(_SC_AVPHYS_PAGES);
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (availablePages > 0 && pageSize > 0) {
            return static_cast<uint64_t>(availablePages) * static_cast<uint64_t>(pageSize);
        }
#endif
        return 0;
    }
};
}
#endif // PROFILER_SERVER_SYSTEM_UTIL_H
