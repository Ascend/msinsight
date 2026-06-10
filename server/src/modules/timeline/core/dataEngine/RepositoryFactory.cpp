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
#include "pch.h"
#include "HardWareRepo.h"
#include "HcclRepo.h"
#include "CcuRepo.h"
#include "OverlapAnsRepo.h"
#include "CANNApiRepo.h"
#include "PythonApiRepo.h"
#include "OSRTApiRepo.h"
#include "MstxRepo.h"
#include "PythonGcRepo.h"
#include "TextRepository.h"
#include "DbFlowRepo.h"
#include "RepositoryFactory.h"
namespace Dic::Module::Timeline {
RepositoryFactory::RepositoryFactory() {
    sliceRespoMap.emplace(PROCESS_TYPE::ASCEND_HARDWARE, std::make_unique<HardWareRepo>());
    sliceRespoMap.emplace(PROCESS_TYPE::HCCL, std::make_unique<HcclRepo>());
    sliceRespoMap.emplace(PROCESS_TYPE::CCU, std::make_unique<CcuRepo>());
    sliceRespoMap.emplace(PROCESS_TYPE::OVERLAP_ANALYSIS, std::make_unique<OverlapAnsRepo>());
    sliceRespoMap.emplace(PROCESS_TYPE::CANN_API, std::make_unique<CANNApiRepo>());
    sliceRespoMap.emplace(PROCESS_TYPE::API, std::make_unique<PythonApiRepo>());
    sliceRespoMap.emplace(PROCESS_TYPE::OSRT_API, std::make_unique<OSRTApiRepo>());
    sliceRespoMap.emplace(PROCESS_TYPE::MS_TX, std::make_unique<MstxRepo>());
    sliceRespoMap.emplace(PROCESS_TYPE::PYTHON_GC, std::make_unique<PythonGcRepo>());
    sliceRespoMap.emplace(PROCESS_TYPE::TEXT, std::make_unique<TextRepository>());
    flowRespoMap.emplace(PROCESS_TYPE::TEXT, std::make_unique<TextRepository>());
    flowRespoMap.emplace(PROCESS_TYPE::DB, std::make_unique<DbFlowRepo>());
    simulationRespoMap.emplace(PROCESS_TYPE::TEXT, std::make_unique<TextRepository>());
};
RepositoryFactory::~RepositoryFactory() {
    sliceRespoMap.clear();
    counterRespoMap.clear();
    flowRespoMap.clear();
    simulationRespoMap.clear();
}

std::shared_ptr<IBaseSliceRepo> RepositoryFactory::GetSliceRespo(PROCESS_TYPE metaType) {
    if (sliceRespoMap.count(metaType) == 0) {
        return nullptr;
    }
    return sliceRespoMap.at(metaType);
}

std::shared_ptr<CounterRepoInterface> RepositoryFactory::GetCounterRespo(PROCESS_TYPE metaType) {
    if (counterRespoMap.count(metaType) == 0) {
        return nullptr;
    }
    return counterRespoMap.at(metaType);
}

std::shared_ptr<FlowRepoInterface> RepositoryFactory::GetFlowRespo(PROCESS_TYPE metaType) {
    if (flowRespoMap.count(metaType) == 0) {
        return nullptr;
    }
    return flowRespoMap.at(metaType);
}

std::shared_ptr<SimulationSliceRepoInterface> RepositoryFactory::GetSimulationSliceRespo(PROCESS_TYPE) {
    return simulationRespoMap.at(PROCESS_TYPE::TEXT);
}
}
