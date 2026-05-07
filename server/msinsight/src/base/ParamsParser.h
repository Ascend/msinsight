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

#ifndef DATA_INSIGHT_CORE_PARAMS_PARSER_H
#define DATA_INSIGHT_CORE_PARAMS_PARSER_H

#include <string>
#include <vector>

namespace Dic {
namespace Server {
using namespace std;

struct ParamsOption {
    int wsPort = -1;
    int logSize = 10 * 1024 * 1024;
    string host = "127.0.0.1";
    string logLevel = "INFO";
#ifdef _WIN32
    string logPath = "";
#else
    string logPath = "./";
#endif
    string eventDir;
    int scanPort{-1};
    bool strictMode{true};
};

class ParamsParser {
public:
    static ParamsParser &Instance();
    const ParamsOption &GetOption() const;
    const std::string &GetError() const;
    bool Parse(const vector<string> &args);
private:
    ParamsParser() = default;
    ~ParamsParser() = default;

    int TryGetPort(const std::string &portStr) const;
    bool ParseField(const std::string &data);
    bool ParseWsPort(const std::string &wsPortStr);
    bool ParseWsHost(const std::string &wsHostStr);
    bool ParseLogPath(const std::string &logPath);
    bool ParseLogSize(const std::string &logSize);
    bool ParseLogLevel(const std::string &logLevel);
    bool ParseEventDir(const string &eventDir);
    void ParseScan(const string &scan);

    const string symbolWsPort = "--wsPort=";
    const string symbolWsHost = "--wsHost=";
    const string symbolLogPath = "--logPath=";
    const string symbolLogSize = "--logSize=";
    const string symbolLogLevel = "--logLevel=";
    const string symbolEventDir = "--eventDir=";
    const string symbolScanPort = "--scanPort="; // vscode插件使用来获取端口
    // 后端启动时如果传入了--notStrict选项，导入文件时不要求权限和属主校验通过，仅在日志中提示
    const string symbolNotStrict = "--notStrict";
    const int minPortNum = 9000;
    const int maxPortNum = 9100;

    const string EQUAL = "=";
    const string SYMBOL_PREFIX = "--";
    const string IP_PATTERN = "^(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})$";

    ParamsOption option;
    std::string error;
};
} // end of namespace Server
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_PARAMS_PARSER_H
