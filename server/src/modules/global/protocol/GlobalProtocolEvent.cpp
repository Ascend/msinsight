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

#include "GlobalProtocolEvent.h"
#include "WsSender.h"

namespace Dic::Protocol {
void SendReadFileFailEvent(const std::string &filePath, const std::string &errMsg) {
    auto event = std::make_unique<ReadFileFailEvent>();
    event->body.filePath = filePath;
    event->body.error = errMsg;
    event->moduleName = Protocol::MODULE_GLOBAL;
    event->result = true;
    Dic::SendEvent(std::move(event));
}
}
