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

#ifndef MSINSIGHT_SERVER_LOG_H
#define MSINSIGHT_SERVER_LOG_H

#include <memory>
#include "mutex"
#include "sstream"
#include "vector"

namespace Dic {
enum class LogLevel : int { L_DEBUG = 0, L_INFO, L_WARN, L_ERROR, L_FATAL };
namespace Server {
class ServerLog {
  public:
    static void Initialize(
        const std::string &logPath, const int &logSize, const std::string &logLevelStr, std::string wsPortStr);

    static std::string GetCurrentLogPath();
    static void SetCurrentLogPath(const std::string &path);

    template <typename... ARGS> static inline void Info(const ARGS... args) {
        Instance().Record(LogLevel::L_INFO, args...);
#ifndef NDEBUG
        Instance().Print(LogLevel::L_INFO, args...);
#endif
    }

    template <typename... ARGS> static inline void Debug(const ARGS... args) {
        Instance().Record(LogLevel::L_DEBUG, args...);
#ifndef NDEBUG
        Instance().Print(LogLevel::L_DEBUG, args...);
#endif
    }

    template <typename... ARGS> static inline void Warn(const ARGS... args) {
        Instance().Record(LogLevel::L_WARN, args...);
#ifndef NDEBUG
        Instance().Print(LogLevel::L_WARN, args...);
#endif
    }

    template <typename... ARGS> static inline void Error(const ARGS... args) {
        Instance().Record(LogLevel::L_ERROR, args...);
#ifndef NDEBUG
        Instance().Print(LogLevel::L_ERROR, args...);
#endif
    }

  private:
    void Initialize(const LogLevel &level);
    ServerLog() = default;
    ~ServerLog() = default;

    static ServerLog &Instance();

    static const std::string GetLogHead(const LogLevel &level);
    static void RecordImpl(const LogLevel &level, std::vector<std::string> &logStrList);
    static void PrintImpl(const LogLevel &level, std::vector<std::string> &logStrList);

    template <typename... ARGS> inline void Record(const LogLevel &level, ARGS &&...args) {
        std::lock_guard<std::mutex> lock(recordInstanceMutex);
        Initialize(level);
        std::vector<std::string> logStrList;
        std::tuple<ARGS &&...> tp(std::forward<ARGS>(args)...);
        GetStringHelper(logStrList, tp, std::make_index_sequence<sizeof...(ARGS)>{});
        RecordImpl(level, logStrList);
    }

    template <typename... ARGS> inline void Print(const LogLevel &level, ARGS &&...args) {
        std::lock_guard<std::mutex> lock(printInstanceMutex);
        Initialize(level);
        std::vector<std::string> logStrList;
        std::tuple<ARGS &&...> tp(std::forward<ARGS>(args)...);
        GetStringHelper(logStrList, tp, std::make_index_sequence<sizeof...(ARGS)>{});
        PrintImpl(level, logStrList);
    }

    template <typename Tuple, size_t... I>
    void GetStringHelper(std::vector<std::string> &logStrList, Tuple &tp, std::index_sequence<I...>) {
        (ConvertString(logStrList, std::get<I>(tp)), ...);
    }
    template <typename T> inline void ConvertString(std::vector<std::string> &logStrList, const T arg) const {
        std::stringstream ss;
        if constexpr (std::is_same_v<T, bool>) {
            // bool类型使用true或false展示，而非0或1
            ss << std::boolalpha << arg;
        } else {
            ss << arg;
        }
        logStrList.emplace_back(ss.str());
    }

    std::mutex recordInstanceMutex;
    std::mutex printInstanceMutex;
    static std::mutex currentLogPathMutex;
    static std::string currentLogPath;
    std::string wsPort;
};
} // end of namespace Server
} // end of namespace Dic

#endif // MSINSIGHT_SERVER_LOG_H
