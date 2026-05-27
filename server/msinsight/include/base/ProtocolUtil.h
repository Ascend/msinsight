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
#ifndef MSINSIGHT_PROTOCOL_UTIL_H
#define MSINSIGHT_PROTOCOL_UTIL_H
#include <string>
#include <map>
#include <functional>
#include <mutex>
#include <optional>
#include <memory>
#include "rapidjson.h"
#include "document.h"
#include "writer.h"
#include "stringbuffer.h"
#include <algorithm>
namespace Dic {
using json_t = rapidjson::Value;
using document_t = rapidjson::Document;
using namespace rapidjson;
const static int UNKNOW_ERROR = 9999;
const std::string MODULE_UNKNOWN = "unknown";
namespace Protocol {
struct ErrorMessage {
    int code = 0;
    std::string message;
};
struct ProtocolMessage {
    enum class Type : int { REQUEST = 0, RESPONSE, EVENT, NONE };
    virtual ~ProtocolMessage() = default;
    unsigned int id = 0;
    ProtocolMessage::Type type = Type::NONE;
    std::string moduleName = MODULE_UNKNOWN;
    std::optional<int> resultCallbackId;
};
struct Request : public ProtocolMessage {
    explicit Request(const std::string &command) : command(command) { type = ProtocolMessage::Type::REQUEST; }
    explicit Request(std::string_view command) : command(std::string(command)) {
        type = ProtocolMessage::Type::REQUEST;
    }
    ~Request() override = default;
    std::string projectName;
    std::string command;
    std::string fileId;
};
struct Response : public ProtocolMessage {
    Response() = default;
    explicit Response(const std::string &command) : command(command) { type = ProtocolMessage::Type::RESPONSE; }
    ~Response() override = default;
    unsigned int requestId = 0;
    bool result = false;
    std::string command;
    std::optional<ErrorMessage> error;
};
struct JsonResponse : public Response {
    explicit JsonResponse(const std::string &command) : Response(command) {}
    [[nodiscard]] virtual std::optional<document_t> ToJson() const = 0;
};
struct JsonSerializable {
  public:
    virtual ~JsonSerializable() = default;
    [[nodiscard]] virtual json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const = 0;
};
struct Event : public ProtocolMessage {
    explicit Event(const std::string &e) : event(e) { type = ProtocolMessage::Type::EVENT; }
    ~Event() override = default;
    std::string event;
    bool result = false;
};
struct JsonEvent : public Event {
    explicit JsonEvent(const std::string &e) : Event(e) {}
    [[nodiscard]] virtual std::optional<document_t> ToJson() const = 0;
};
class ProtocolUtil {
  public:
    ProtocolUtil() = default;
    virtual ~ProtocolUtil() = default;

    void Register();
    void UnRegister();

    std::unique_ptr<Request> FromJson(const json_t &requestJson, std::string &error);
    std::optional<document_t> ToJson(const Response &response, std::string &error);
    std::optional<document_t> ToJson(const Event &event, std::string &error);

    // set base info
    // request
    static bool SetRequestBaseInfo(Request &request, const json_t &json);
    // response
    static void SetResponseJsonBaseInfo(const Response &response, document_t &json);
    // event
    static void SetEventJsonBaseInfo(const Event &event, document_t &json);

    // common json to request
    template <class SubRequest>
    static std::unique_ptr<Request> BuildRequestFromJson(const json_t &json, std::string &error) {
        static_assert(std::is_same_v<std::unique_ptr<Request>, decltype(SubRequest::FromJson(json, error))>,
            "SubRequest must have a static FromJson method returning std::unique_ptr<Request>");
        return SubRequest::FromJson(json, error);
    }
    // response to json
    static std::optional<document_t> CommonResponseToJson(const Response &response) {
        try {
            const auto &jsonResponse = dynamic_cast<const JsonResponse &>(response);
            return jsonResponse.ToJson();
        } catch (const std::bad_cast &e) {
            return std::nullopt;
        }
    }
    // event to json
    static std::optional<document_t> CommonEventToJson(const Event &event) {
        try {
            const auto &jsonResponse = dynamic_cast<const JsonEvent &>(event);
            return jsonResponse.ToJson();
        } catch (const std::bad_cast &e) {
            return std::nullopt;
        }
    }

  protected:
    std::mutex mutex;
    using JsonToRequestFunc = std::function<std::unique_ptr<Request>(const json_t &, std::string &error)>;
    using ResponseToJsonFunc = std::function<std::optional<document_t>(const Response &)>;
    using EventToJsonFunc = std::function<std::optional<document_t>(const Event &)>;
    std::map<std::string, JsonToRequestFunc> jsonToReqFactory;
    std::map<std::string, ResponseToJsonFunc> resToJsonFactory;
    std::map<std::string, EventToJsonFunc> eventToJsonFactory;

  private:
    virtual void RegisterJsonToRequestFuncs() = 0;
    virtual void RegisterResponseToJsonFuncs() = 0;
    virtual void RegisterEventToJsonFuncs() = 0;

    // request
    static bool IsRequest(const json_t &jsonRequest);
    static std::string Command(const json_t &jsonRequest);
    std::optional<JsonToRequestFunc> GetJsonToRequestFunc(const std::string &command);
    // response
    std::optional<ResponseToJsonFunc> GetResponseToJsonFunc(const std::string &command);
    // event
    std::optional<EventToJsonFunc> GetEventToJsonFunc(const std::string &event);
};
} // namespace Protocol
} // namespace Dic

#endif // MSINSIGHT_PROTOCOL_UTIL_H
