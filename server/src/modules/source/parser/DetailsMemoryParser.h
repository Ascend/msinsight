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

#ifndef INSTRUCTION_TIMELINE_PARSER_H
#define INSTRUCTION_TIMELINE_PARSER_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include "SourceProtocolResponse.h"
#include "SourceProtocol.h"

namespace Dic::Module::Source {
class DetailsMemoryParser {
  public:
    bool GetDetailsMemoryGraph(const std::string &targetBlockId, Protocol::DetailsMemoryGraphResBody &responseBody,
        std::string &curFilePath, std::map<int, std::vector<Position>> &curBlockMap);
    bool GetDetailsMemoryTable(const std::string &targetBlockId, Protocol::DetailsMemoryTableResBody &responseBody,
        std::string &curFilePath, std::map<int, std::vector<Position>> &curBlockMap);
    bool GetDetailsBaseInfo(Protocol::DetailsBaseInfoResBody &responseBody, std::string &curFilePath,
        std::map<int, std::vector<Position>> &curBlockMap);
    bool GetDetailsLoadInfo(Protocol::DetailsLoadInfoResBody &responseBody, std::string &curFilePath,
        std::map<int, std::vector<Position>> &curBlockMap);

  protected:
    static Protocol::MemoryGraph ParseJsonToMemoryGraph(const json_t &json);
    static Protocol::MemoryTable ParseJsonToMemoryTable(const json_t &json);
    static Protocol::UtilizationRate ParseJsonToUtilizationRate(const json_t &json);
    static Protocol::DetailsBaseInfoResBody ParseJsonToBaseInfo(const document_t &json);
    static std::string GetUnitType(int64_t unitTypeNumber);
    static Protocol::CompareData<Protocol::SubBlockUnitData> ParseSubBlockUnitData(const json_t &item);
    std::optional<Protocol::SubBlockData> ConvertStrToSubBlockData(const std::string &str);
};

} // Dic
// Module
// Source

#endif // INSTRUCTION_TIMELINE_PARSER_H
