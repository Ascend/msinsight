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

#ifndef PROFILER_SERVER_REPOSITORYFACTORY_H
#define PROFILER_SERVER_REPOSITORYFACTORY_H
#include <unordered_map>
#include <memory>
#include "RepositoryFactoryInterface.h"
#include "SliceRepoInterface.h"
#include "CounterRepoInterface.h"
#include "DomainObject.h"

namespace Dic::Module::Timeline {
class RepositoryFactory : public RepositoryFactoryInterface {
  public:
    static std::shared_ptr<RepositoryFactory> Instance() {
        static std::shared_ptr<RepositoryFactory> instance = std::make_shared<RepositoryFactory>();
        return instance;
    }
    RepositoryFactory();
    RepositoryFactory(const RepositoryFactory &) = delete;
    RepositoryFactory &operator=(const RepositoryFactory &) = delete;
    RepositoryFactory(RepositoryFactory &&) = delete;
    RepositoryFactory &operator=(RepositoryFactory &&) = delete;
    std::shared_ptr<IBaseSliceRepo> GetSliceRespo(PROCESS_TYPE) override;
    std::shared_ptr<CounterRepoInterface> GetCounterRespo(PROCESS_TYPE) override;
    std::shared_ptr<FlowRepoInterface> GetFlowRespo(PROCESS_TYPE) override;
    std::shared_ptr<SimulationSliceRepoInterface> GetSimulationSliceRespo(PROCESS_TYPE) override;
    ~RepositoryFactory() override;

  private:
    std::unordered_map<PROCESS_TYPE, std::shared_ptr<IBaseSliceRepo>> sliceRespoMap;
    std::unordered_map<PROCESS_TYPE, std::shared_ptr<CounterRepoInterface>> counterRespoMap;
    std::unordered_map<PROCESS_TYPE, std::shared_ptr<FlowRepoInterface>> flowRespoMap;
    std::unordered_map<PROCESS_TYPE, std::shared_ptr<SimulationSliceRepoInterface>> simulationRespoMap;
};
}

#endif // PROFILER_SERVER_REPOSITORYFACTORY_H
