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

#ifndef PROFILER_SERVER_PARSEUNITMANAGER_H
#define PROFILER_SERVER_PARSEUNITMANAGER_H

#include <map>
#include "AbstractParseUnit.h"
namespace Dic::Module::FullDb {
class ParseUnitManager {
  public:
    using Creator = std::function<std::unique_ptr<IAbstractParseUnit>()>;
    static ParseUnitManager &Instance();

    ParseUnitManager(const ParseUnitManager &) = delete;
    ParseUnitManager &operator=(const ParseUnitManager &) = delete;
    ParseUnitManager(ParseUnitManager &&) = delete;
    ParseUnitManager &operator=(ParseUnitManager &&) = delete;

    void ExecuteUnitList(const ParseUnitParams &params, const std::vector<std::string> &unitNameList);
    void RegisterUnit(const std::string &name, Creator unit);

  private:
    ParseUnitManager() = default;
    ~ParseUnitManager() = default;
    std::map<std::string, Creator> unitMap;
};

template <typename T> class ParseUnitRegistrar {
  public:
    explicit ParseUnitRegistrar(const std::string &name) {
        ParseUnitManager::Instance().RegisterUnit(name, []() { return std::make_unique<T>(); });
    }
};
}
#endif // PROFILER_SERVER_PARSEUNITMANAGER_H
