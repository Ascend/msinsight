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

#ifndef DATA_INSIGHT_CORE_BASE_SERVER_H
#define DATA_INSIGHT_CORE_BASE_SERVER_H

#include <string>
#include "WsSessionImpl.h"

namespace Dic {
namespace Server {
class BaseServer {
  public:
    BaseServer(const std::string &host, int port) : host(host), port(port) {}
    virtual ~BaseServer() = default;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    const std::string &GetHost() const { return host; }
    int GetPort() const { return port; };

  protected:
    std::string host = "localhost";
    int port;
};
} // end of namespace Server
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_BASE_SERVER_H
