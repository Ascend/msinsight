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
#ifndef PROFILER_SERVER_DATAENGINEINTERFACE_H
#define PROFILER_SERVER_DATAENGINEINTERFACE_H
#include "RepositoryFactoryInterface.h"
#include "SliceRepoInterface.h"
#include "CounterRepoInterface.h"
#include "SimulationSliceRepoInterface.h"

namespace Dic::Module::Timeline {
class DataEngineInterface : public IBaseSliceRepo,
                            public IPythonFuncSlice,
                            public ITextSlice,
                            public IFindSliceByTimepointAndName,
                            public IFindSliceByNameList,
                            public FlowRepoInterface,
                            public SimulationSliceRepoInterface {
  public:
    ~DataEngineInterface() override = default;
    virtual void SetRepositoryFactory(std::shared_ptr<RepositoryFactoryInterface>) = 0;
};
}

#endif // PROFILER_SERVER_DATAENGINEINTERFACE_H
