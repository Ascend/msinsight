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
#ifndef MSINSIGHT_WSSENDER_H
#define MSINSIGHT_WSSENDER_H
#include "ProtocolUtil.h"
namespace Dic {
void SendEvent(std::unique_ptr<Dic::Protocol::Event> eventPtr);
void SendResponse(std::unique_ptr<Protocol::Response> responsePtr, bool result, const std::string &errorMsg = "",
    const int errorCode = UNKNOW_ERROR);
}
#endif // MSINSIGHT_WSSENDER_H
