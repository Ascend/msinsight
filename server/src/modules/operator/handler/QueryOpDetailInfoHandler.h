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

#ifndef PROFILER_SERVER_QUERYOPDETAILINFOHANDLER_H
#define PROFILER_SERVER_QUERYOPDETAILINFOHANDLER_H

#include "ModuleRequestHandler.h"
#include "OperatorRequestHandler.h"
#include "OperatorProtocolRequest.h"
#include "OperatorProtocolResponse.h"

namespace Dic::Module::Operator {
class QueryOpDetailInfoHandler : public OperatorRequestHandler {
  public:
    QueryOpDetailInfoHandler() { command = REQ_RES_OPERATOR_DETAIL_INFO; }

    ~QueryOpDetailInfoHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
    bool HandleCompareDataRequest(OperatorDetailInfoRequest &request, OperatorDetailInfoResponse &response);

  private:
    bool HandleDetailDataRequest(OperatorDetailInfoRequest &request, OperatorDetailInfoResponse &response);
    std::vector<Protocol::OperatorDetailCmpInfoRes> GetCmpDataVec(
        std::vector<Protocol::OperatorDetailInfoRes> &baseDbData,
        std::vector<Protocol::OperatorDetailInfoRes> &cmpDbData);
    std::vector<Protocol::OperatorDetailCmpInfoRes> GetFixNumDiffCmpData(
        std::vector<Protocol::OperatorDetailCmpInfoRes> &datailData, Protocol::OperatorStatisticReqParams &reqParams,
        const int64_t total, const std::set<std::string> &basePmuHeader, const std::set<std::string> &cmpPmuHeader);
    void SortDataBynameAndStartTime(std::vector<Protocol::OperatorDetailInfoRes> &baseDbData,
        std::vector<Protocol::OperatorDetailInfoRes> &cmpDbData);
    void FromatDatailData(Protocol::OperatorDetailCmpInfoRes &data, const std::set<std::string> &baseDiff,
        const std::set<std::string> &cmpDiff, const std::set<std::string> intersection);
    std::string CalPmuDataCompare(const std::string &comPmu, const std::string &basePmu);
};
}

#endif // PROFILER_SERVER_QUERYOPDETAILINFOHANDLER_H
