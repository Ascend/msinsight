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

#include "../utils/pch.h"
#include "ParamsParser.h"

namespace Dic {
namespace Server {
const int INVALID_NUMBER = 0xffffffff;

ParamsParser &ParamsParser::Instance() {
    static ParamsParser instance;
    return instance;
}

const ParamsOption &ParamsParser::GetOption() const { return option; }

const std::string &ParamsParser::GetError() const { return error; }

bool ParamsParser::ParseField(const std::string &data) {
    std::string::size_type pos = data.find(symbolWsPort);
    if (pos != std::string::npos) {
        return ParseWsPort(data.substr(pos + symbolWsPort.length()));
    }
    pos = data.find(symbolWsHost);
    if (pos != std::string::npos) {
        return ParseWsHost(data.substr(pos + symbolWsHost.length()));
    }
    pos = data.find(symbolLogPath);
    if (pos != std::string::npos) {
        return ParseLogPath(data.substr(pos + symbolLogPath.length()));
    }
    pos = data.find(symbolLogLevel);
    if (pos != std::string::npos) {
        return ParseLogLevel(data.substr(pos + symbolLogLevel.length()));
    }
    pos = data.find(symbolLogSize);
    if (pos != std::string::npos) {
        return ParseLogSize(data.substr(pos + symbolLogSize.length()));
    }

    pos = data.find(symbolEventDir);
    if (pos != std::string::npos) {
        return ParseEventDir(data.substr(pos + symbolEventDir.length()));
    }
    pos = data.find(symbolScanPort);
    if (pos != std::string::npos) {
        ParseScan(data.substr(pos + symbolScanPort.length()));
        return true;
    }
    if (data == symbolNotStrict) {
        option.strictMode = false;
        return true;
    }
    error = "ERROR: " + data + " has not been supported.";
    return false;
}

bool ParamsParser::Parse(const vector<string> &args) {
    if (args.size() <= 1) {
        error = "ERROR: Startup parameter count is not enough.";
        return false;
    }
    for (uint32_t i = 1; i < args.size(); i++) {
        std::string data = args.at(i);
        if (!StringUtil::Contains(args.at(i), EQUAL) && i < args.size() - 1 &&
            !StringUtil::StartWith(args.at(i + 1), SYMBOL_PREFIX)) {
            data.append(EQUAL).append(args.at(++i));
        }
        if (!this->ParseField(data)) {
            return false;
        }
    }
    return true;
}

int ParamsParser::TryGetPort(const std::string &portStr) const {
    int port = NumberUtil::TryParseInt(portStr);
    if ((port < minPortNum) || (port > maxPortNum)) {
        return -1;
    }
    return port;
}

bool ParamsParser::ParseWsPort(const std::string &wsPortStr) {
    int port = TryGetPort(wsPortStr);
    if (port <= 0) {
        error =
            "ERROR: Port error, port range is " + std::to_string(minPortNum) + "-" + std::to_string(maxPortNum) + ".";
        return false;
    }
    option.wsPort = port;
    return true;
}

bool ParamsParser::ParseWsHost(const std::string &wsHostStr) {
    if (RegexUtil::RegexMatch(wsHostStr, IP_PATTERN)) {
        option.host = wsHostStr;
        return true;
    }
    error = "ERROR: Host is not valid.";
    return false;
}

bool ParamsParser::ParseLogPath(const std::string &logPath) {
    if (!logPath.empty()) {
        option.logPath = logPath;
    }
    return true;
}

bool ParamsParser::ParseLogSize(const std::string &logSize) {
    int size = NumberUtil::TryParseInt(logSize);
    if (size != INVALID_NUMBER) {
        option.logSize = size;
    }
    return true;
}

bool ParamsParser::ParseLogLevel(const std::string &logLevel) {
    if (!logLevel.empty()) {
        option.logLevel = logLevel;
    }
    return true;
}

bool ParamsParser::ParseEventDir(const string &eventDir) {
    if (!eventDir.empty()) {
        option.eventDir = eventDir;
    }
    return true;
}

void ParamsParser::ParseScan(const string &scan) {
    if (!scan.empty()) {
        option.scanPort = NumberUtil::TryParseInt(scan);
    }
}
} // end of namespace Server
} // end of namespace Dic
