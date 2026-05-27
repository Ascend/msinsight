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
#include <cstdio>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace Dic {
class SocketUtil {
  public:
    static bool PortIsUsed(int port) {
#ifdef _WIN32
        WSADATA wsaData;
        static const int VERSION_HIGH = 2;
        static const int VERSION_LOW = 2;
        if (WSAStartup(MAKEWORD(VERSION_HIGH, VERSION_LOW), &wsaData) != 0) {
            return false;
        }
#endif
        auto soc = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        sockaddr_in sin{};
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
#ifdef _WIN32
        sin.sin_addr.S_un.S_addr = INADDR_ANY;
        int flag = 1;
        setsockopt(soc, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&flag, sizeof(flag));
        auto res = bind(soc, (struct sockaddr *)&sin, sizeof(sin));
        closesocket(soc);
        WSACleanup();
#else
        sin.sin_addr.s_addr = INADDR_ANY;
        auto res = bind(soc, (struct sockaddr *)&sin, sizeof(sin));
        close(soc);
#endif
        return res == -1;
    }
};
}
