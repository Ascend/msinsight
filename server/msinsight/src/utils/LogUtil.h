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

#ifndef DATA_INSIGHT_CORE_LOG_H
#define DATA_INSIGHT_CORE_LOG_H

#include <iostream>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <mutex>
#ifdef __APPLE__
#include <filesystem>
#include <sys/stat.h>
#elif __linux__
#include <sys/stat.h>
#endif
#include "TimeUtil.h"
#include "ServerLog.h"
#include "StringUtil.h"
#include "FileUtil.h"

namespace Dic {
enum class LogOutType : int { TERMINAL = 0, FILE, BOTH };
class LogPrefix {
  public:
    static LogPrefix &Instance() {
        static LogPrefix instance;
        return instance;
    }

    inline const std::string TimePrefix(const TimeStyle &style = TimeStyle::WITH_MILLI_SEC) const {
        return TimeUtil::Instance().NowStr(style);
    }

    inline const std::string LevelPrefix(const LogLevel &level) const {
        static std::map<LogLevel, std::string> map = {{LogLevel::L_INFO, "[Info]"}, {LogLevel::L_WARN, "[Warn]"},
            {LogLevel::L_ERROR, "[Error]"}, {LogLevel::L_FATAL, "[Fatal]"}, {LogLevel::L_DEBUG, "[Debug]"}};
        if (map.count(level) == 0) {
            return map[LogLevel::L_INFO];
        }
        return map[level];
    }

    inline const std::string LocationPrefix(const std::string &file, const std::string &func, const int &line) const {
        std::stringstream ss;
        ss << " (" << file << ", " << func << ", line: " << line << ") |";
        return ss.str();
    }

  private:
    LogPrefix() = default;
    ~LogPrefix() = default;
};

class LogUtil {
  public:
    LogUtil(const LogOutType &type, const std::string &filePath, const std::string &wsPort)
        : originFilePath(filePath), outType(type), wsPort(wsPort) {
        Initialize();
    }

    LogUtil(const LogOutType &type, const std::string &filePath, const std::string &wsPort, int maxSize)
        : maxSize(maxSize), originFilePath(filePath), outType(type), wsPort(wsPort) {
        Initialize();
    }

    ~LogUtil() { Destroy(); }

    inline LogUtil &SetOutType(const LogOutType &type) {
        if (this->outType != type) {
            this->outType = type;
        }
        return *this;
    }

    inline LogUtil &SetFilePath(const std::string &logFilePath) {
        if (this->filePath != logFilePath) {
            this->filePath = logFilePath;
        }
        return *this;
    }

    inline LogUtil &SetLogLevel(const LogLevel &logLevel) {
        if (this->level != logLevel) {
            this->level = logLevel;
        }
        return *this;
    }

    inline LogUtil &SetWsPort(const std::string &port) {
        this->wsPort = port;
        return *this;
    }

    inline LogUtil &SetMaxSize(const int logMaxSize) {
        if (this->maxSize != logMaxSize) {
            this->maxSize = logMaxSize;
        }
        return *this;
    }

    template <typename... ARGS> inline LogUtil &operator<<(const ARGS... args) {
        LogT(args...);
        return *this;
    }

    void LogT(const LogLevel &logLevel, const std::string &head, std::vector<std::string> &logStrList) {
        if (logLevel < level) {
            return;
        }
        std::string str = head;
        if (!logStrList.empty() && logStrList[0].find('%') != std::string::npos) {
            str += FormatString(logStrList);
        } else {
            for (const auto &item : logStrList) {
                str.append(item);
            }
        }
        LogStr(str);
    }

    inline std::string GetLogFilePath() const { return this->filePath; }

    template <typename T, typename... ARGS> inline void LogT(const LogLevel &logLevel, const T &t, const ARGS... args) {
        if (logLevel < level) {
            return;
        }
        FormatLogT(t, args...);
    }

    template <typename Head, typename Format> inline void FormatLogT(const Head &head, const Format &format) {
        std::string str = GetString(head, format);
        LogStr(str);
    }

    template <typename Head, typename Format, typename... ARGS>
    inline void FormatLogT(const Head &head, const Format &format, const ARGS... args) {
        std::string str = GetString(head);
        std::string formatStr = GetString(format);
        if (formatStr.find('%') != std::string::npos) {
            str += FormatString(formatStr, args...);
        } else {
            str += GetString(format, args...);
        }
        LogStr(str);
    }

    static LogLevel GetLogLevel(const std::string &logLevel) {
        static std::map<std::string, LogLevel> map = {{"INFO", LogLevel::L_INFO}, {"WARN", LogLevel::L_WARN},
            {"ERROR", LogLevel::L_ERROR}, {"FATAL", LogLevel::L_FATAL}, {"DEBUG", LogLevel::L_DEBUG}};
        if (map.count(logLevel) != 0) {
            return map.at(logLevel);
        }
        return LogLevel::L_INFO;
    }

  private:
    void Initialize() {
        // read the last write log file and the count
        while (outType != LogOutType::TERMINAL && CheckRotating()) {
            RotatingLogFile();
        }
    }

    inline std::string SanitizeLogMessage(const std::string &message) const {
        std::string sanitized;
        for (char c : message) {
            switch (c) {
            case '\n':
                sanitized += "\\n";
                break;
            case '\r':
                sanitized += "\\r";
                break;
            case '\f':
                sanitized += "\\f";
                break;
            case '\b':
                sanitized += "\\b";
                break;
            case '\v':
                sanitized += "\\v";
                break;
            case '\u007F':
                sanitized += "\\u007F";
                break;
            case '\t':
                sanitized += "\\t";
                break;
            case '"':
                sanitized += "\\\"";
                break;
            case '\'':
                sanitized += "\\'";
                break;
            case '\\':
                sanitized += "\\\\";
                break;
            case '%':
                sanitized += "\\%";
                break;
            case '<':
                sanitized += "\\<";
                break;
            case '>':
                sanitized += "\\>";
                break;
            case '|':
                sanitized += "\\|";
                break;
            case '&':
                sanitized += "\\&";
                break;
            case '$':
                sanitized += "\\$";
                break;
            default:
                sanitized += c;
                break;
            }
        }
        return sanitized;
    }

    // Function to format the string with multiple parameters
    template <typename... Args> inline std::string FormatString(const std::string &format, Args... args) {
        std::ostringstream oss;
        std::array<std::string, sizeof...(args)> arr = {GetString(args)...};
        for (auto &arg : arr) {
            arg = SanitizeLogMessage(arg);
        }
        size_t start = 0;
        size_t pos = 0;
        size_t argIndex = 0;
        while ((pos = format.find('%', start)) != std::string::npos) {
            oss << format.substr(start, pos - start);
            if (argIndex < arr.size()) {
                oss << arr[argIndex++];
            }
            start = pos + 1;
        }
        oss << format.substr(start);
        return oss.str();
    }

    inline std::string FormatString(std::vector<std::string> &logStrList) {
        std::ostringstream oss;
        std::string format = logStrList[0];
        for (auto &arg : logStrList) {
            arg = SanitizeLogMessage(arg);
        }
        size_t start = 0;
        size_t pos = 0;
        size_t argIndex = 1;
        while ((pos = format.find('%', start)) != std::string::npos) {
            oss << format.substr(start, pos - start);
            if (argIndex < logStrList.size()) {
                oss << logStrList[argIndex++];
            }
            start = pos + 1;
        }
        oss << format.substr(start);
        return oss.str();
    }

    void Destroy() {
        if (ofs.is_open()) {
            ofs.close();
        }
    }

    template <typename T> inline std::string GetString(const T &t) const {
        std::stringstream ss;
        ss << t;
        return ss.str();
    }

    template <typename T, typename... ARGS> inline std::string GetString(const T &t, const ARGS... args) const {
        std::string str = GetString(t);
        str += GetString(args...);
        return str;
    }

    static inline int GetFileSize(const std::string &path) {
        std::ifstream fr;
        fr.open(path, std::ios::in | std::ios::binary);
        int result = 0;
        if (fr.is_open()) {
            fr.seekg(0, std::ifstream::end);
            result = static_cast<int>(fr.tellg());
        }
        fr.close();
        return result;
    }

    inline void RotatingLogFile() {
        if (ofs.is_open()) {
            ofs.close();
        }
        filePath = originFilePath;
        if (count >= maxCount) {
            count = 0;
            isAppend = false;
        }
        filePath.insert(filePath.find_last_of('.'), "_" + wsPort + "_" + std::to_string(++count));
        if (isAppend) {
            currentSize = GetFileSize(filePath);
            ofs.open(filePath, std::ios::out | std::ios::app);
        } else {
            ofs.open(filePath, std::ios::out | std::ios::trunc);
            currentSize = 0;
        }
#if defined(__linux__) || defined(__APPLE__)
        ofs.close(); // 关闭文件以设置权限
        mode_t mode = 0640; // 业务数据权限要求设置为0640 （rw-r-----）
        FileUtil::ModifyFilePermissions(filePath, mode);
        ofs.open(filePath, std::ofstream::out | std::ofstream::app); // 重新打开文件
#endif
    }

    inline bool CheckRotating() const {
        // count == 0 is true when init
        return (count == 0) || currentSize >= maxSize;
    }

    /**
     * @brief force flush the log content to disk
     */
    inline void Flush() {
        if (!wsPort.empty() && wsPort != "-1" && ofs.is_open()) {
            ofs.close();
            currentSize = GetFileSize(filePath);
            ofs.open(filePath, std::ios::out | std::ios::app);
        }
    }

    inline void Append(const std::string &str) {
        if (ofs.is_open()) {
            ofs << str << std::endl;
            ofs.flush();
            if (str.length() >= INT_MAX) {
                return;
            }
            int strLen = static_cast<int>(str.length());
            if (currentSize <= INT_MAX - strLen) {
                currentSize += strLen;
            }
        }
    }

    inline void LogStr(const std::string &str) {
        if (outType == LogOutType::BOTH || outType == LogOutType::TERMINAL) {
            (*outMap[level]) << str << std::endl;
        }
        if (outType == LogOutType::BOTH || outType == LogOutType::FILE) {
            if (CheckRotating()) {
                RotatingLogFile();
            }
            if (FileUtil::IsSoftLink(filePath)) {
                return;
            }
            // ensure the prev log write into disk
            Flush();
            Append(str);
        }
    }

    template <typename T> inline void LogT(const T &t) { LogStr(GetString(t)); }

    int maxSize = 10 * 1024 * 1024;
    const int maxCount = 10;
    volatile int currentSize = 0;
    volatile int count = 0;
    volatile bool isAppend = true;
    std::ofstream ofs;
    std::string originFilePath;
    std::string filePath;
    LogOutType outType = LogOutType::BOTH;
    LogLevel level = LogLevel::L_INFO;
    std::string wsPort;
    std::map<LogLevel, std::ostream *> outMap = {{LogLevel::L_INFO, &std::cout}, {LogLevel::L_WARN, &std::cout},
        {LogLevel::L_DEBUG, &std::cout}, {LogLevel::L_ERROR, &std::cerr}, {LogLevel::L_FATAL, &std::cerr}};
};
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_LOG_H
