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

#ifndef BINFILEPARSEUTIL_H
#define BINFILEPARSEUTIL_H

#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "SourceProtocol.h"

namespace Dic::Module::Source {
class BinFileParseUtil {
  public:
    static std::string GetContentStr(std::ifstream &file, const Position &position, uint64_t maxSize = MAX_DATA_SIZE);
    static bool IsDataSizeExceedUpperLimit(uint64_t realSize, uint64_t upperLimit);
    static std::string GetSingleContentStrByDataType(
        std::ifstream &file, DataTypeEnum dataTypeEnum, const std::map<int, std::vector<Position>> &curBlockMap);

  private:
    static constexpr uint64_t MAX_DATA_SIZE = 1024 * 1024 * 100; // 100MB
};
}

#endif // BINFILEPARSEUTIL_H
