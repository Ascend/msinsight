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

#ifndef PROFILER_SERVER_GLOBAL_PROTOCOL_H
#define PROFILER_SERVER_GLOBAL_PROTOCOL_H

#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
class GlobalProtocol : public ProtocolUtil {
  public:
    GlobalProtocol() = default;
    ~GlobalProtocol() override = default;

  private:
    void RegisterJsonToRequestFuncs() override;
    void RegisterResponseToJsonFuncs() override;
    void RegisterEventToJsonFuncs() override;

    // json to request
    static std::unique_ptr<Request> ToFilesGetRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToHeartCheckRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToProjectExplorerUpdateRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToProjectExplorerInfoGetRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToProjectExplorerInfoDeleteRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToProjectExplorerInfoClearRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToProjectValidCheckRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToSetBaselineRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToCancelBaselineRequest(const json_t &json, std::string &error);
    // response to json
    static std::optional<document_t> ToFilesGetResponseJson(const Response &response);
    static std::optional<document_t> ToTokenHeartCheckResponseJson(const Response &response);
    static std::optional<document_t> ToProjectExplorerInfoUpdateResponseJson(const Response &response);
    static std::optional<document_t> ToProjectExplorerInfoGetResponseJson(const Response &response);
    static std::optional<document_t> ToProjectExplorerInfoDeleteResponseJson(const Response &response);
    static std::optional<document_t> ToProjectExplorerInfoClearResponseJson(const Response &response);
    static std::optional<document_t> ToProjectValidCheckResponseJson(const Response &response);
    static std::optional<document_t> ToSetBaselineResponseJson(const Response &response);
    static std::optional<document_t> ToCancelBaselineResponseJson(const Response &response);
    static std::optional<document_t> ToGetModuleConfigResponseJson(const Response &response);
    // event to json
    static std::optional<document_t> ToReadFileFailEventJson(const Event &event);
};
} // namespace Protocol
} // namespace Dic

#endif // PROFILER_SERVER_GLOBAL_PROTOCOL_H
