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

#ifndef DATA_INSIGHT_CORE_MODULE_MANAGER_H
#define DATA_INSIGHT_CORE_MODULE_MANAGER_H

#include <memory>
#include <mutex>
#include "ProtocolMessage.h"
#include "BaseModule.h"

namespace Dic {
namespace Module {
class ModuleManager {
  public:
    static ModuleManager &Instance();
    void OnDispatchModuleRequest(std::unique_ptr<Request> request);

  private:
    ModuleManager();
    ~ModuleManager();

    void Register();
    void UnRegister();

    std::mutex mutex;
    std::map<std::string, std::unique_ptr<BaseModule>> moduleMap;
};
} // end of namespace Module
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_MODULE_MANAGER_H
