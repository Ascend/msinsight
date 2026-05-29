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

#ifndef PROFILER_SERVER_PROTOCOL_MANAGER_H
#define PROFILER_SERVER_PROTOCOL_MANAGER_H

#include <string>
#include <map>
#include <mutex>
#include <optional>
#include <memory>
#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
class ProtocolManager {
  public:
    static ProtocolManager &Instance() {
        static ProtocolManager instance;
        return instance;
    }

    std::unique_ptr<Request> FromJson(const std::string &requestStr, std::string &error);
    std::optional<document_t> ToJson(const Response &response, std::string &error);
    std::optional<document_t> ToJson(const Event &event, std::string &error);

  private:
    ProtocolManager();
    ~ProtocolManager();

    void Register();
    void UnRegister();

    std::mutex mutex;
    std::map<std::string, std::unique_ptr<ProtocolUtil>> protocolMap;
};
} // namespace Protocol
} // namespace Dic
#endif // PROFILER_SERVER_PROTOCOL_MANAGER_H
