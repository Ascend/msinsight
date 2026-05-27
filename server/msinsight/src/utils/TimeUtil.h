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

#ifndef DATA_INSIGHT_CORE_TIME_UTIL_H
#define DATA_INSIGHT_CORE_TIME_UTIL_H

#include <chrono>
#include <memory>
#include <sstream>
#include <functional>
#include <map>
#include "StringUtil.h"

namespace Dic {
struct Time {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int millisecond = 0;
};

enum class TimeStyle : int {
    // YYYY-mm-DD HH:MM:SS.sss
    WITH_MILLI_SEC = 0,
    // YYYYmmDDHHMMSSsss
    WITH_SEC_NO_SPLIT,
};

class TimeUtil {
  public:
    static TimeUtil &Instance() {
        static TimeUtil instance;
        return instance;
    }

    const Time Now() const {
        using namespace std::chrono;
        constexpr int sinceYear = 1900;
        constexpr int sinceMonth = 1;
        constexpr int msDuration = 1000;
        system_clock::time_point now = system_clock::now();
        auto durationInMs = duration_cast<milliseconds>(now.time_since_epoch());
        time_t tt = system_clock::to_time_t(now);
        struct tm nowTime;
#ifdef _WIN32
        localtime_s(&nowTime, &tt);
#else
        localtime_r(&tt, &nowTime);
#endif
        Time time = {.year = nowTime.tm_year + sinceYear,
            .month = nowTime.tm_mon + sinceMonth,
            .day = nowTime.tm_mday,
            .hour = nowTime.tm_hour,
            .minute = nowTime.tm_min,
            .second = nowTime.tm_sec,
            .millisecond = static_cast<int>(durationInMs.count() % msDuration)};
        return time;
    }

    // LCOV_EXCL_BR_START
    /* *
     * get now UTC seconds
     *
     * @return UTC seconds
     */
    uint32_t NowUTC() const {
        using std::chrono::system_clock;
        system_clock::time_point now = system_clock::now();
        time_t tt = system_clock::to_time_t(now);
        return tt;
    }

    const std::string NowStr(const TimeStyle &style = TimeStyle::WITH_MILLI_SEC) const {
        using TimeStyleFunc = std::function<std::string(const Time &time)>;
        static std::map<TimeStyle, TimeStyleFunc> funcMap = {{TimeStyle::WITH_MILLI_SEC, GetTimeStyleWithMilliSec},
            {TimeStyle::WITH_SEC_NO_SPLIT, &GetTimeStyleWithSecNoSplit}};
        if (funcMap.count(style) == 0) {
            return "";
        }
        return funcMap[style](Now());
    }

  private:
    TimeUtil() = default;
    ~TimeUtil() = default;

    static const std::string GetTimeStyleWithMilliSec(const Time &time) {
        std::string timeStr = StringUtil::IntToString(time.year, 4) + "-" + StringUtil::IntToString(time.month, 2) +
            "-" + StringUtil::IntToString(time.day, 2) + " " + StringUtil::IntToString(time.hour, 2) + ":" +
            StringUtil::IntToString(time.minute, 2) + ":" + StringUtil::IntToString(time.second, 2) + "." +
            StringUtil::IntToString(time.millisecond, 3);
        return timeStr;
    }

    static std::string GetTimeStyleWithSecNoSplit(const Time &time) {
        std::string timeStr = StringUtil::IntToString(time.year, 4) + StringUtil::IntToString(time.month, 2) +
            StringUtil::IntToString(time.day, 2) + StringUtil::IntToString(time.hour, 2) +
            StringUtil::IntToString(time.minute, 2) + StringUtil::IntToString(time.second, 2) +
            StringUtil::IntToString(time.millisecond, 3);
        return timeStr;
    }
    // LCOV_EXCL_BR_STOP
};
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_TIME_UTIL_H
