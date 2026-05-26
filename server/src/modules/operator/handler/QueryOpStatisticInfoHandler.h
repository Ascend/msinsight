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

#ifndef PROFILER_SERVER_QUERYOPSTATISTICINFOHANDLER_H
#define PROFILER_SERVER_QUERYOPSTATISTICINFOHANDLER_H

#include "ModuleRequestHandler.h"
#include "OperatorRequestHandler.h"
#include "OperatorProtocolRequest.h"
#include "OperatorProtocolResponse.h"

namespace Dic::Module::Operator {
using OpStaticResVec = std::vector<Protocol::OperatorStatisticInfoRes>;

class QueryOpStatisticInfoHandler : public OperatorRequestHandler {
  public:
    QueryOpStatisticInfoHandler() { command = REQ_RES_OPERATOR_STATISTIC_INFO; }

    ~QueryOpStatisticInfoHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
    bool HandleCompareDataRequest(OperatorStatisticInfoRequest &request, OperatorStatisticInfoResponse &response);

  private:
    std::vector<Protocol::OperatorStatisticCmpInfoRes> GetCmpDataVec(
        std::string &group, OpStaticResVec &base, OpStaticResVec &cmp);
    std::string GetGroup(const std::string &paramsGroup, OperatorStatisticInfoRes &data);
    bool HandleStatisticcDataRequest(OperatorStatisticInfoRequest &request, OperatorStatisticInfoResponse &response);
    void GroupingData(const std::string &paramsGroup, OpStaticResVec &datFromDb,
        std::map<std::string, Protocol::OperatorStatisticCmpInfoRes> &groupMap, bool isBaselineData);
    void SetOpInputShapeGroupData(OperatorStatisticCmpInfoRes &data);
    void SetOpOrHcclTypeGroupData(OperatorStatisticCmpInfoRes &data);
    void CalDiffDataByGroup(const std::string &paramsGroup, OperatorStatisticCmpInfoRes &data);
    std::string CalDataCompare(const std::string &com, const std::string &base);
    std::vector<Protocol::OperatorStatisticCmpInfoRes> GetFixNumDiffCmpData(
        std::vector<Protocol::OperatorStatisticCmpInfoRes> &statisticData,
        Protocol::OperatorStatisticReqParams &reqParams, const int64_t total);
};
}
#endif // PROFILER_SERVER_QUERYOPSTATISTICINFOHANDLER_H
