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

#ifndef PROFILER_SERVER_COMMUNICATIONRAPIDSAXHANDLER_H
#define PROFILER_SERVER_COMMUNICATIONRAPIDSAXHANDLER_H

#include <iostream>
#include "encodings.h"
#include "reader.h"
#include "document.h"
#include "VirtualClusterDatabase.h"
#include "GlobalDefs.h"
#include "DataBaseManager.h"
#include "CommonRapidHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {

class CommunicationRapidSaxHandler
    : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, CommunicationRapidSaxHandler>,
      CommonRapidHandler {
  public:
    explicit CommunicationRapidSaxHandler(std::shared_ptr<TextClusterDatabase> database, const std::string &uniqueKey);
    ~CommunicationRapidSaxHandler();
    bool Null();
    bool Bool(bool b);
    bool Int(int i);
    bool Uint(unsigned u);
    bool Int64(int64_t i);
    bool Uint64(uint64_t u);
    bool Double(double d);
    bool RawNumber(const Ch *str, SizeType len, bool copy);
    bool String(const char *str, rapidjson::SizeType length, bool copy);
    bool StartObject();
    bool Key(const char *str, rapidjson::SizeType length, bool copy);
    bool EndObject(rapidjson::SizeType memberCount);
    bool StartArray();
    bool EndArray(rapidjson::SizeType elementCount);
    void GetBandwidth();
    void GetTimeInfo();

  private:
    std::string currentKey;
    std::string rankId;
    std::string stepId;
    std::string stageId;
    std::string tempOpName;
    std::string transportType;
    std::string tableFlag;
    std::string selectedPath;
    uint32_t currentDepth = 0;
    uint32_t stageIdDepth = 1;
    uint32_t stepIdDepth = 2;
    uint32_t tempOpNameDepth = 3;
    uint32_t rankIdDepth = 4;
    uint32_t stepSubLen = 4;
    uint32_t tableFlagDepth = 5;
    uint32_t infoDepth = 6;
    uint32_t infoDepthSeven = 7;
    uint32_t sizeDistributionDepth = 8;
    std::string exception;
    std::string uniqueKey;

    CommunicationBandWidth bandwidth;
    CommunicationTimeInfo timeInfo;

    std::unordered_map<std::string, CommunicationTimeInfo> timeOpTotalInfoMap;
    std::unordered_map<std::string, CommunicationBandWidth> bandwidthOpTotalInfoMap;

    std::string GenerateTimeInfoKey(const CommunicationTimeInfo &info) const;
    void StatTimeTotalOpInfo(const CommunicationTimeInfo &info);
    std::string GenerateBandwidthInfoKey(const CommunicationBandWidth &info) const;
    void StatBandwidthTotalOpInfo(const CommunicationBandWidth &info);
    std::unordered_map<std::string, PackageInfo> TransStrToPackageMap(const std::string &str);
    std::string TransPackageMapToStr(std::unordered_map<std::string, PackageInfo> &packageMap);
    void DealData();
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_COMMUNICATIONRAPIDSAXHANDLER_H
