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
#include "DataEngine.h"

namespace Dic::Module::Timeline {
void DataEngine::SetRepositoryFactory(std::shared_ptr<RepositoryFactoryInterface> repositoryFactoryInterface) {
    respotoryFactory = std::move(repositoryFactoryInterface);
}

/**
 * 根据trackId查询简单算子
 * @param sliceQuery
 * @param sliceVec
 */
void DataEngine::QuerySimpleSliceWithOutNameByTrackId(
    const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query simple slice.Data engine not assembly");
        return;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(sliceQuery.metaType);
    if (sliceRespo == nullptr) {
        return;
    }
    sliceRespo->QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
    std::sort(sliceVec.begin(), sliceVec.end());
}

void DataEngine::QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query slice ids.Data engine not assembly");
        return;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(sliceQuery.metaType);
    if (sliceRespo == nullptr) {
        return;
    }
    const auto pythonFuncSliceRepo = dynamic_cast<IPythonFuncSlice *>(sliceRespo.get());
    if (pythonFuncSliceRepo == nullptr) {
        return;
    }
    pythonFuncSliceRepo->QuerySliceIdsByCat(sliceQuery, sliceIds);
}

bool DataEngine::QuerySliceByCatAndTimeRange(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query slice by cat and range.Data engine not assembly");
        return false;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(sliceQuery.metaType);
    if (sliceRespo == nullptr) {
        return false;
    }
    const auto pythonFuncSliceRepo = dynamic_cast<IPythonFuncSlice *>(sliceRespo.get());
    if (pythonFuncSliceRepo == nullptr) {
        return false;
    }
    const bool isSupported = pythonFuncSliceRepo->QuerySliceByCatAndTimeRange(sliceQuery, sliceVec);
    if (isSupported) {
        std::sort(sliceVec.begin(), sliceVec.end(), SliceDomain::CompareTimestampASC);
    }
    return isSupported;
}

uint64_t DataEngine::QueryPythonFunctionCountByTrackId(const SliceQuery &sliceQuery) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query python function.Data engine not assembly");
        return 0;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(sliceQuery.metaType);
    if (sliceRespo == nullptr) {
        return 0;
    }
    const auto pythonFuncSliceRepo = dynamic_cast<IPythonFuncSlice *>(sliceRespo.get());
    if (pythonFuncSliceRepo == nullptr) {
        return 0;
    }
    const uint64_t count = pythonFuncSliceRepo->QueryPythonFunctionCountByTrackId(sliceQuery);
    return count;
}

void DataEngine::QueryCompeteSliceVecByTimeRangeAndTrackId(
    const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query compete slice by range.Data engine not assembly");
        return;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(sliceQuery.metaType);
    if (sliceRespo == nullptr) {
        return;
    }
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(sliceRespo.get());
    if (textSliceRepo == nullptr) {
        return;
    }
    textSliceRepo->QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
}

void DataEngine::QueryFlowPointByTimeRange(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query flow point by range.Data engine not assembly");
        return;
    }
    auto flowRespo = respotoryFactory->GetFlowRespo(flowQuery.metaType);
    if (flowRespo == nullptr) {
        return;
    }
    flowRespo->QueryFlowPointByTimeRange(flowQuery, flowPointVec);
}

void DataEngine::QueryFlowPointByFlowId(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query flow point by flow id.Data engine not assembly");
        return;
    }
    auto flowRespo = respotoryFactory->GetFlowRespo(flowQuery.metaType);
    if (flowRespo == nullptr) {
        return;
    }
    flowRespo->QueryFlowPointByFlowId(flowQuery, flowPointVec);
}

void DataEngine::QueryAllThreadInfo(
    const ThreadQuery &threadQuery, std::unordered_map<uint64_t, std::pair<std::string, std::string>> &threadInfo) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query thread info.Data engine not assembly");
        return;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(threadQuery.metaType);
    if (sliceRespo == nullptr) {
        return;
    }
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(sliceRespo.get());
    if (textSliceRepo == nullptr) {
        return;
    }
    textSliceRepo->QueryAllThreadInfo(threadQuery, threadInfo);
}

/**
 * 根据Id集合查询完整算子信息
 * @param sliceQuery
 * @param sliceIds
 * @param CompeteSliceVec
 */
void DataEngine::QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query compete slice.Data engine not assembly");
        return;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(sliceQuery.metaType);
    if (sliceRespo == nullptr) {
        return;
    }
    sliceRespo->QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
}

void DataEngine::QueryFlowPointByCategory(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query flow point.Data engine not assembly");
        return;
    }
    auto textFlowRespo = respotoryFactory->GetFlowRespo(PROCESS_TYPE::TEXT);
    if (textFlowRespo == nullptr) {
        return;
    }
    textFlowRespo->QueryFlowPointByCategory(flowQuery, flowPointVec);
    if (!std::empty(flowPointVec)) {
        return;
    }
    auto DbFlowRepo = respotoryFactory->GetFlowRespo(PROCESS_TYPE::DB);
    if (DbFlowRepo == nullptr) {
        return;
    }
    DbFlowRepo->QueryFlowPointByCategory(flowQuery, flowPointVec);
}

void DataEngine::QueryAllFlagSlice(
    const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &competeSliceDomainVec) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query slice flag.Data engine not assembly");
        return;
    }
    auto textRepo = respotoryFactory->GetSimulationSliceRespo(PROCESS_TYPE::TEXT);
    if (textRepo == nullptr) {
        return;
    }
    textRepo->QueryAllFlagSlice(sliceQuery, competeSliceDomainVec);
}

bool DataEngine::QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query slice detail.Data engine not assembly");
        return false;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(sliceQuery.metaType);
    if (sliceRespo == nullptr) {
        return false;
    }
    return sliceRespo->QuerySliceDetailInfo(sliceQuery, competeSliceDomain);
}

bool DataEngine::QuerySliceByTimepointAndName(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query slice by time point.Data engine not assembly");
        return false;
    }
    if (sliceQuery.metaType != PROCESS_TYPE::TEXT && sliceQuery.metaType != PROCESS_TYPE::API) {
        Server::ServerLog::Warn("Failed to query slice by time point.meta type is wrong!");
        return false;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(sliceQuery.metaType);
    if (sliceRespo == nullptr) {
        return false;
    }
    const auto findByTimepointAndNameSliceRepo = dynamic_cast<IFindSliceByTimepointAndName *>(sliceRespo.get());
    if (findByTimepointAndNameSliceRepo == nullptr) {
        return false;
    }
    return findByTimepointAndNameSliceRepo->QuerySliceByTimepointAndName(sliceQuery, competeSliceDomain);
}

bool DataEngine::QuerySliceDetailInfoByNameList(
    const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) {
    if (respotoryFactory == nullptr) {
        Server::ServerLog::Warn("Failed to query slice by name list.The data engine is not assembled.");
        return false;
    }
    auto sliceRespo = respotoryFactory->GetSliceRespo(params.metaType);
    if (sliceRespo == nullptr) {
        return false;
    }
    const auto findByNameListSliceRepo = dynamic_cast<IFindSliceByNameList *>(sliceRespo.get());
    if (findByNameListSliceRepo == nullptr) {
        return false;
    }
    return findByNameListSliceRepo->QuerySliceDetailInfoByNameList(params, res);
}
}
