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

#ifndef PROFILER_SERVER_EVENT_UTIL_H
#define PROFILER_SERVER_EVENT_UTIL_H

#include <string>
#include <map>
#include <optional>
#include <memory>
#include <functional>
#include "GlobalDefs.h"
#include "EventDef.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Trace;
class EventUtil {
  public:
    static EventUtil &Instance() {
        static EventUtil instance;
        return instance;
    }
    using json_t = rapidjson::Value;
    static std::string Type(const json_t &json);
    Trace::Event *FromJson(const json_t &json, const std::string &type);

  private:
    EventUtil();
    ~EventUtil();

    void Register();
    void UnRegister();

    using JsonToEventFunc = std::function<Trace::Event *(const json_t &)>;
    std::map<std::string, JsonToEventFunc> jsonToEventFactory;
    std::optional<JsonToEventFunc> GetJsonToEventFunc(const std::string &type);
    static Trace::Event *ToSliceEvent(const json_t &json);
    static Trace::Event *ToSimulationSliceEvent(const json_t &json);
    static Trace::Event *ToSimulationBeginSliceEvent(const json_t &json);
    static Trace::Event *ToSimulationEndSliceEvent(const json_t &json);
    static Trace::Event *ToMetaDataEvent(const json_t &json);
    static Trace::Event *ToFlowEvent(const json_t &json);
    static Trace::Event *ToCounterEvent(const json_t &json);
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
#endif // PROFILER_SERVER_EVENT_UTIL_H
