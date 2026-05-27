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

#ifndef PROFILER_SERVER_ROOFLINEPARSER_H
#define PROFILER_SERVER_ROOFLINEPARSER_H

#include <memory>
#include "RooflineParserImpl.h"
#include "SourceProtocolUtil.h"

namespace Dic::Module::Source {
class RooflineParser {
  public:
    RooflineParser() : impl(std::make_unique<RooflineParserImpl>()) {};

    bool GetDetailsRoofline(const std::string &jsonStr, Protocol::DetailsRooflineBody &responseBody) {
        if (impl != nullptr) {
            return impl->GetDetailsRoofline(jsonStr, responseBody);
        }
        return false;
    }

  private:
    std::unique_ptr<RooflineParserImpl> impl;
};
}

#endif // PROFILER_SERVER_ROOFLINEPARSER_H
