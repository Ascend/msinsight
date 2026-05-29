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

#ifndef DATA_INSIGHT_CORE_SERVER_MESSAGE_BUFFER_H
#define DATA_INSIGHT_CORE_SERVER_MESSAGE_BUFFER_H

#include <mutex>
#include <memory>
#include <limits>
#include "ProtocolUtil.h"

namespace Dic {
namespace Protocol {
class ProtocolMessageBuffer {
  public:
    ProtocolMessageBuffer() = default;
    ~ProtocolMessageBuffer() = default;

    ProtocolMessageBuffer &operator<<(const std::string &data);
    std::unique_ptr<Protocol::ProtocolMessage> Pop();
    void Clear();

  private:
    uint64_t GetBodyLength(const uint64_t &headPosition, const uint64_t &headLength) const;

    inline const static std::string REQ_DELIMITER = "\r\n\r\n";
    inline const static std::string HEAD_START = "Content-Length:";
    const uint32_t matchMinNum = 2;
    const uint64_t bufferLimit = 16 * 1024 * 1024;
    const uint64_t invalidBodyLen = std::numeric_limits<uint64_t>::max();
    std::mutex mutex;
    std::string buffer;
    std::string error;
    uint64_t headPos = 0;
    uint64_t headLen = 0;
    uint64_t bodyPos = 0;
    uint64_t bodyLen = 0;
};
} // end of namespace Protocol
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_SERVER_MESSAGE_BUFFER_H
