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

#ifndef PROFILER_SERVER_ROOFLINEPARSERIMPL_H
#define PROFILER_SERVER_ROOFLINEPARSERIMPL_H
#include <optional>
#include <string>
#include "SourceProtocolUtil.h"
namespace Dic::Module::Source {
class RooflineParserImpl {
  public:
    bool GetDetailsRoofline(const std::string &jsonStr, Protocol::DetailsRooflineBody &responseBody);

    static Protocol::SubBlockUnitData ParseSubBlockUnitData(const json_t &item);

    static Protocol::RooflineGraph ParseRooflineData(const json_t &item);

    static Protocol::Roofline ParseRoofline(const json_t &item);

    static std::optional<Protocol::RooflineData> ConvertStrToRooflineData(const std::string &json);
};
}

#endif // PROFILER_SERVER_ROOFLINEPARSERIMPL_H
