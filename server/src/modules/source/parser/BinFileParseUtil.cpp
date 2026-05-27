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

#include "BinFileParseUtil.h"
#include "ServerLog.h"
#include "FileUtil.h"
#include "NumberSafeUtil.h"

namespace Dic::Module::Source {
using namespace Dic::Server;
std::string BinFileParseUtil::GetContentStr(std::ifstream &file, const Position &position, uint64_t maxSize) {
    if (!file) {
        return "";
    }
    int64_t start = position.startPos;
    int64_t end = position.endPos;
    if (start >= end || start < 0) {
        ServerLog::Error(
            "Invalid start position % and end position % when get content string from bin file", start, end);
        return "";
    }
    int64_t dataSize = NumberSafe::Sub(end, start);
    if (IsDataSizeExceedUpperLimit(dataSize, maxSize)) {
        ServerLog::Error("Data size of content exceeds % bytes when get content string from bin file", maxSize);
        return "";
    }

    file.seekg(start, std::ios::beg);

    std::string jsonStr;
    jsonStr.resize(dataSize);
    file.read(&jsonStr[0], dataSize);
    if (!file) {
        ServerLog::Error("Failed to read content str.");
        return "";
    }
    jsonStr = StringUtil::ToLocalStr(jsonStr);
    return jsonStr;
}

bool BinFileParseUtil::IsDataSizeExceedUpperLimit(uint64_t realSize, uint64_t upperLimit) {
    return realSize > upperLimit;
}

std::string BinFileParseUtil::GetSingleContentStrByDataType(
    std::ifstream &file, DataTypeEnum dataTypeEnum, const std::map<int, std::vector<Position>> &curBlockMap) {
    if (!file.is_open()) {
        return "";
    }
    if (curBlockMap.find(static_cast<int>(dataTypeEnum)) == curBlockMap.end()) {
        return "";
    }
    // 从文件获取内容
    const std::vector<Position> &baseInfoPos = curBlockMap.at(static_cast<int>(dataTypeEnum));
    if (baseInfoPos.empty()) {
        return "";
    }
    return BinFileParseUtil::GetContentStr(file, baseInfoPos[0]);
}
} // Dic::Module::Source
