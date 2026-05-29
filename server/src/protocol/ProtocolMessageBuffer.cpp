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

#include "pch.h"
#include "ProtocolEnumUtil.h"
#include "ProtocolManager.h"
#include "ProtocolMessageBuffer.h"

namespace Dic {
namespace Protocol {
using namespace Dic::Server;

uint64_t ProtocolMessageBuffer::GetBodyLength(const uint64_t &headPosition, const uint64_t &headLength) const {
    std::string lenStr = buffer.substr(headPosition, headLength);
    std::optional<std::smatch> matchRes = RegexUtil::RegexMatch(lenStr, "Content-Length:\\s*(\\d+)");
    if (!matchRes.has_value() || matchRes.value().size() < matchMinNum) {
        return invalidBodyLen;
    }
    uint64_t res;
    try {
        res = std::stoull(matchRes.value()[1].str());
    } catch (std::invalid_argument &) {
        res = invalidBodyLen;
    } catch (std::out_of_range &) {
        res = invalidBodyLen;
    } catch (...) {
        res = invalidBodyLen;
    }
    return res;
}

ProtocolMessageBuffer &ProtocolMessageBuffer::operator<<(const std::string &data) {
    std::unique_lock<std::mutex> lock(mutex);
    if (data.find(HEAD_START) != std::string::npos || data.find(REQ_DELIMITER) != std::string::npos) {
        return *this;
    }
    std::string dataLengthStr = std::to_string(data.length());
    uint64_t completeDataLength = HEAD_START.length() + dataLengthStr.length() + REQ_DELIMITER.length() + data.length();
    if (completeDataLength + buffer.size() > bufferLimit) {
        ServerLog::Warn("Request is too long or too many");
        return *this;
    }
    buffer.append(HEAD_START);
    buffer.append(dataLengthStr);
    buffer.append(REQ_DELIMITER);
    buffer.append(data);
    return *this;
}

std::unique_ptr<ProtocolMessage> ProtocolMessageBuffer::Pop() {
    std::unique_lock<std::mutex> lock(mutex);
    headPos = buffer.find(HEAD_START);
    if (headPos == std::string::npos) {
        return nullptr;
    }
    if (headPos != 0) {
        buffer = buffer.substr(headPos);
        headPos = 0;
    }
    uint64_t splitPos = buffer.find(REQ_DELIMITER);
    if (splitPos == std::string::npos) {
        return nullptr;
    }
    headLen = splitPos - headPos;
    bodyLen = GetBodyLength(headPos, headLen);
    bodyPos = splitPos + REQ_DELIMITER.length();
    // Prevent integer overflow
    std::string::size_type msgLen = headLen + bodyLen;
    if ((bodyLen == invalidBodyLen) || (buffer.length() < msgLen)) {
        return nullptr;
    }
    std::string bodyStr = buffer.substr(bodyPos, bodyLen);
    bodyStr = StringUtil::ToLocalStr(bodyStr);
    std::unique_ptr<Request> request = ProtocolManager::Instance().FromJson(bodyStr, error);
    if (request == nullptr) {
        Server::ServerLog::Warn("Dispatch failed requests, detail: %", error);
        // 从buffer中删除无法处理的请求数据
        buffer = buffer.substr(bodyPos + bodyLen);
        return nullptr;
    }
    buffer = buffer.substr(bodyPos + bodyLen); // buffer removes head and body string
    return std::unique_ptr<ProtocolMessage>(request.release());
}

void ProtocolMessageBuffer::Clear() {
    std::unique_lock<std::mutex> lock(mutex);
    buffer.clear();
}
} // end of namespace Protocol
} // end of namespace Dic
