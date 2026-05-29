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

#ifndef PROFILER_SERVER_REPOSITORYFACTORYINTERFACE_H
#define PROFILER_SERVER_REPOSITORYFACTORYINTERFACE_H
#include <memory>
#include "SliceRepoInterface.h"
#include "CounterRepoInterface.h"
#include "FlowRepoInterface.h"
#include "SimulationSliceRepoInterface.h"
#include "DomainObject.h"
namespace Dic::Module::Timeline {
class RepositoryFactoryInterface {
  public:
    virtual ~RepositoryFactoryInterface() = default;
    virtual std::shared_ptr<IBaseSliceRepo> GetSliceRespo(PROCESS_TYPE) = 0;
    virtual std::shared_ptr<SimulationSliceRepoInterface> GetSimulationSliceRespo(PROCESS_TYPE) = 0;
    virtual std::shared_ptr<CounterRepoInterface> GetCounterRespo(PROCESS_TYPE) = 0;
    virtual std::shared_ptr<FlowRepoInterface> GetFlowRespo(PROCESS_TYPE) = 0;
};
}

#endif // PROFILER_SERVER_REPOSITORYFACTORYINTERFACE_H
