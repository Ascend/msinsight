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

#ifndef PROFILER_SERVER_ADVISORPROTOCOLFROMREQUESTJSON_H
#define PROFILER_SERVER_ADVISORPROTOCOLFROMREQUESTJSON_H

#include "GlobalDefs.h"
#include "AdvisorProtocolRequest.h"

namespace Dic::Protocol {
template <typename RequestType> std::unique_ptr<Request> ToRequest(const Dic::json_t &json, std::string &error);
class AdvisorProtocolFromRequestJson {
  public:
    static std::unique_ptr<Request> ToAffinityOptimizerRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToAffinityAPIRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToOperatorFusionRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToAICpuOperatorRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToAclnnOperatorRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToOperatorDispatchRequest(const json_t &json, std::string &error);
};
}

#endif // PROFILER_SERVER_ADVISORPROTOCOLFROMREQUESTJSON_H
