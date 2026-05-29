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

#ifndef PROFILER_SERVER_COMMUNICATIONMATRIXRAPIDHANDLER_H
#define PROFILER_SERVER_COMMUNICATIONMATRIXRAPIDHANDLER_H

#include <iostream>
#include "encodings.h"
#include "reader.h"
#include "document.h"
#include "VirtualClusterDatabase.h"
#include "GlobalDefs.h"
#include "CommonRapidHandler.h"
#include "DataBaseManager.h"

namespace Dic {
namespace Module {
namespace Timeline {
class CommunicationMatrixRapidHandler
    : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, CommunicationMatrixRapidHandler>,
      public CommonRapidHandler {
  public:
    explicit CommunicationMatrixRapidHandler(
        std::shared_ptr<TextClusterDatabase> database, const std::string &uniqueKey);
    ~CommunicationMatrixRapidHandler();
    bool Null();
    bool Bool(bool bl);
    bool Int(int i);
    bool Uint(unsigned uint);
    bool Int64(int64_t i);
    bool Uint64(uint64_t u);
    bool Double(double doubleVal);
    bool String(const char *val, rapidjson::SizeType length, bool copy);
    bool StartObject();
    bool Key(const char *keyStr, rapidjson::SizeType length, bool copy);
    bool EndObject(rapidjson::SizeType memberCount);
    bool StartArray();
    bool EndArray(rapidjson::SizeType elementCount);
    CommunicationMatrixInfo MapToMatrixInfo(const rapidjson::Document &json);

  private:
    std::string exception;
    uint32_t currentDepth = 0;
    rapidjson::Document currentObject;
    std::string currentKey;
    std::string groupId;
    std::string iterationId;
    std::string tempOpName;
    std::string tempRank;
    uint32_t groupDepth = 1;
    uint32_t stepDepth = 2;
    uint32_t opNameDepth = 3;
    uint32_t ranksDepth = 4;
    uint32_t stepSubLen = 4;
    std::string uniqueKey;
    std::unordered_map<std::string, CommunicationMatrixInfo> matrixTotalOpInfoMap;

    void StatTotalOpInfo(const CommunicationMatrixInfo &matrixInfo);
    std::string GenerateMatrixKey(const CommunicationMatrixInfo &matrixInfo) const;
};

} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_COMMUNICATIONMATRIXRAPIDHANDLER_H
