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

#ifndef PROFILER_SERVER_IEPROTOCOL_H
#define PROFILER_SERVER_IEPROTOCOL_H
#include <memory>
#include <optional>
#include <string>
#include "GlobalDefs.h"
#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
class IEProtocol : public ProtocolUtil {
  public:
    IEProtocol() = default;

    ~IEProtocol() override = default;

  private:
    void RegisterJsonToRequestFuncs() override;

    void RegisterResponseToJsonFuncs() override;

    void RegisterEventToJsonFuncs() override;

    static std::unique_ptr<Request> ToIEUsageViewRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToIETableRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToIEGroupRequest(const json_t &json, std::string &error);

    static std::optional<document_t> ToIEUsageViewResponseJson(const Response &response);
    static std::optional<document_t> ToIETableViewResponseJson(const Response &response);
    static std::optional<document_t> ToIEGroupResponseJson(const Response &response);

    static std::optional<document_t> ToParseIECompletedEventJson(const Event &event);
};
}
}
#endif // PROFILER_SERVER_IEPROTOCOL_H
