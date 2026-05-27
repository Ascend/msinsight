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
#ifndef DIC_PROTOCOL_ENUM_UTIL_H
#define DIC_PROTOCOL_ENUM_UTIL_H
#include "ProtocolUtil.h"
namespace Dic::Protocol {
const std::string REQUEST_NAME = "request";
const std::string RESPONSE_NAME = "response";
const std::string EVENT_NAME = "event";
enum class LinkType : int { SOCKET = 0, WEBSOCKET };
template <typename ENUM> using EnumStrMap = std::map<ENUM, std::string>;
const EnumStrMap<ProtocolMessage::Type> PROTOCOL_MESSAGE_TYPE_ES = {{ProtocolMessage::Type::REQUEST, REQUEST_NAME},
    {ProtocolMessage::Type::RESPONSE, RESPONSE_NAME}, {ProtocolMessage::Type::EVENT, EVENT_NAME}};
const EnumStrMap<Protocol::LinkType> LINK_TYPE_ES = {
    {Protocol::LinkType::WEBSOCKET, "websocket"}, {Protocol::LinkType::SOCKET, "socket"}};
template <typename E> std::optional<std::string> ENUM_TO_STR(const E &e) { return std::nullopt; }
template <typename E> std::optional<E> STR_TO_ENUM(const std::string &s) { return std::nullopt; }
template <typename E, typename M> std::optional<E> TryGetEnum(const M &map, const std::string &s) {
    for (const auto &iter : map) {
        if (iter.second == s) {
            return iter.first;
        }
    }
    return std::nullopt;
}
template <> inline std::optional<std::string> ENUM_TO_STR<ProtocolMessage::Type>(const ProtocolMessage::Type &e) {
    return PROTOCOL_MESSAGE_TYPE_ES.count(e) == 0 ? std::nullopt : std::make_optional<>(PROTOCOL_MESSAGE_TYPE_ES.at(e));
}
template <> inline std::optional<ProtocolMessage::Type> STR_TO_ENUM<ProtocolMessage::Type>(const std::string &s) {
    return TryGetEnum<ProtocolMessage::Type>(PROTOCOL_MESSAGE_TYPE_ES, s);
}
template <> inline std::optional<std::string> ENUM_TO_STR<Protocol::LinkType>(const Protocol::LinkType &e) {
    return LINK_TYPE_ES.count(e) == 0 ? std::nullopt : std::make_optional<>(LINK_TYPE_ES.at(e));
}
template <> inline std::optional<Protocol::LinkType> STR_TO_ENUM<Protocol::LinkType>(const std::string &s) {
    return TryGetEnum<Protocol::LinkType>(LINK_TYPE_ES, s);
}
} // end of namespace Protocol
#endif // DIC_PROTOCOL_ENUM_UTIL_H
