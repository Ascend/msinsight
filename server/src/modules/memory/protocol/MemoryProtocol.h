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

#ifndef PROFILER_SERVER_MEMORYPROTOCOL_H
#define PROFILER_SERVER_MEMORYPROTOCOL_H
#include <memory>
#include <optional>
#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
class MemoryProtocol : public ProtocolUtil {
  public:
    MemoryProtocol() = default;
    ~MemoryProtocol() override = default;

  private:
    void RegisterJsonToRequestFuncs() override;
    void RegisterResponseToJsonFuncs() override;
    void RegisterEventToJsonFuncs() override;

    // json to request
    static std::unique_ptr<Request> ToMemoryTypeRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMemoryResourceTypeRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMemoryOperatorRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMemoryComponentRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMemoryViewRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMemoryFindSliceRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMemoryOperatorSizeRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMemoryStaticOperatorGraphRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMemoryStaticOperatorSizeRequest(const json_t &json, std::string &error);
    // response to json
    static std::optional<document_t> ToMemoryTypeResponseJson(const Response &response);
    static std::optional<document_t> ToMemoryResourceTypeResponseJson(const Response &response);
    static std::optional<document_t> ToMemoryOperatorResponseJson(const Response &response);
    static std::optional<document_t> ToMemoryComponentResponseJson(const Response &response);
    static std::optional<document_t> ToMemoryViewResponseJson(const Response &response);
    static std::optional<document_t> ToMemoryFindSliceResponseJson(const Response &response);
    static std::optional<document_t> ToMemoryOperatorSizeResponseJson(const Response &response);
    static std::optional<document_t> ToMemoryStaticOperatorGraphResponseJson(const Response &response);
    static std::optional<document_t> ToMemoryStaticOperatorSizeResponseJson(const Response &response);
};

} // end of namespace Protocol
} // end of namespace Dic

#endif // PROFILER_SERVER_MEMORYPROTOCOL_H
