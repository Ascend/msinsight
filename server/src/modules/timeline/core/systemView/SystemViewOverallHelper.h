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

#ifndef PROFILER_SERVER_SYSTEMVIEWOVERALLHELPER_H
#define PROFILER_SERVER_SYSTEMVIEWOVERALLHELPER_H

#include <string>
#include <vector>
#include "StringUtil.h"
#include "ServerLog.h"
#include "TimelineProtocolResponse.h"

using namespace Dic::Protocol;
namespace Dic::Module::Timeline {
// Overall Metrics分类名称
const std::string OVERALL_CAT_PAGED_ATTENTION = "Paged Attention";
const std::string OVERALL_CAT_SDMA = "SDMA";
const std::string OVERALL_CAT_TENSOR_MOVE = "Tensor Move";
const std::string OVERALL_CAT_FLASH = "Flash Attention";
const std::string OVERALL_CAT_MATMUL = "Matmul";
const std::string OVERALL_CAT_CONV = "Conv";
const std::string OVERALL_CAT_FORWARD = "Forward";
const std::string OVERALL_CAT_BACKWARD = "Backward";
const std::string OVERALL_CAT_CUBE = "Cube";
const std::string OVERALL_CAT_VECTOR = "Vector";
const std::string OVERALL_CAT_OTHER_CUBE = "Other Cube";
const std::string OVERALL_CAT_OTHER_VECTOR = "Other Vector";
const std::string OVERALL_CAT_TRANS = "Trans";
const std::string OVERALL_CAT_NO_TRANS = "No Trans";
const std::string OVERALL_CAT_COMPUTING = "Computing Time";
const std::string OVERALL_CAT_E2E = "E2E Time";
const std::string OVERALL_CAT_OTHER = "Other";

// 按python api名称过滤下发NPU算子
const std::vector<std::string> OVERALL_FLASH_MASK = {
    "flash_attention", "fusion_attention", "flashattn", "xformers_flash", "efficient_attention", "flsh2atten"};
const std::vector<std::string> OVERALL_MATMUL_MASK = {"aten::addmm", "aten::bmm", "aten::mm", "aten::matmul"};
const std::string OVERALL_CONV_MASK = "aten::conv";
const std::vector<std::string> OVERALL_BWD_MASK = {"bwd", "backward", "back", "grad"};

// 按算子名称opName或类型opType过滤NPU算子
const std::string OVERALL_PAGED_ATTENTION_OP_MASK = "pagedattention";
const std::string OVERALL_SDMA_OP_MASK = "aclnninplacecopy";
const std::string OVERALL_TENSOR_MOVE_OP_MASK = "tensormove";
const std::string OVERALL_FLASH_ATTENTION_OP_MASK = "flashattention";
const std::string OVERALL_CONV_OP_MASK = "conv";
const std::string OVERALL_MATMUL_OP_MASK = "matmul";
const std::vector<std::string> OVERALL_BWD_OP_MASK = {"bwd", "grad"};
const std::vector<std::string> OVERALL_TRANS_MASK = {"cast", "transdata", "transpose"};

struct CpuCubeOpInfo {
    std::string pythonApi;
    bool isCubeOp = false;
    uint64_t trackId{};
    uint64_t start{};
    uint64_t end{};

    void CheckCubeOp() {
        std::string nameLower = StringUtil::ToLower(pythonApi);
        if (StringUtil::ContainAnyOfSubStr(nameLower, OVERALL_FLASH_MASK) || // Flash Attention相关api
            StringUtil::ContainAnyOfSubStr(nameLower, OVERALL_MATMUL_MASK) || // Matmul相关api
            StringUtil::StartWith(nameLower, OVERALL_CONV_MASK)) { // Conv相关api
            isCubeOp = true;
        }
    }
};

struct OverallTmpInfo {
    std::string opName;
    std::string opType;
    std::string pythonApi;
    bool isInBwdTrack = false;
    uint64_t startTime{};
    double duration{};
    uint64_t flowStartTime{};
    double cubeTime{};
    double aicoreTime{};
    double macTime{};
    // 多级目录
    std::vector<std::string> categoryList;

    // 判断categoryList目录列表是否符合expectList（对比范围仅限expectList.size()），符合则返回true
    bool IsCategoryListEqual(const std::vector<std::string> &expectList) const {
        if (expectList.size() > categoryList.size()) {
            return false;
        }
        return std::equal(expectList.begin(), expectList.end(), categoryList.begin());
    }

    bool operator<(const OverallTmpInfo &other) const { return flowStartTime < other.flowStartTime; }

    void GetKernelCategories() {
        std::string lowOpName = StringUtil::ToLower(opName);
        std::string lowOpType = StringUtil::ToLower(opType);
        std::string lowPy = StringUtil::ToLower(pythonApi);
        if (StringUtil::Contains(lowOpType, OVERALL_PAGED_ATTENTION_OP_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_PAGED_ATTENTION);
        } else if (StringUtil::StartWith(lowOpName, OVERALL_SDMA_OP_MASK) &&
            StringUtil::Contains(lowOpName, OVERALL_TENSOR_MOVE_OP_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_SDMA);
            categoryList.emplace_back(OVERALL_CAT_TENSOR_MOVE);
        } else if (flowStartTime != 0 && !pythonApi.empty()) {
            GetCategoriesByCpuOp(lowPy);
        } else if (StringUtil::Contains(lowOpType, OVERALL_FLASH_ATTENTION_OP_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_FLASH);
            GetFwdBwdByOpType(lowOpType);
            categoryList.emplace_back(OVERALL_CAT_CUBE);
        } else if (StringUtil::StartWith(lowOpType, OVERALL_CONV_OP_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_CONV);
            GetFwdBwd(lowOpType);
            categoryList.emplace_back(OVERALL_CAT_CUBE);
        } else if (StringUtil::Contains(lowOpType, OVERALL_MATMUL_OP_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_MATMUL);
            categoryList.emplace_back(OVERALL_CAT_CUBE);
        } else if (macTime > 0 || aicoreTime > 0 || cubeTime > 0) {
            categoryList.emplace_back(OVERALL_CAT_OTHER_CUBE);
        } else {
            categoryList.emplace_back(OVERALL_CAT_OTHER_VECTOR);
            if (StringUtil::ContainAnyOfSubStr(lowOpType, OVERALL_TRANS_MASK)) {
                categoryList.emplace_back(OVERALL_CAT_TRANS);
            } else {
                categoryList.emplace_back(OVERALL_CAT_NO_TRANS);
            }
        }
    }

    void GetCategoriesByCpuOp(const std::string &lowPy) {
        // flash attention相关 api
        if (StringUtil::ContainAnyOfSubStr(lowPy, OVERALL_FLASH_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_FLASH);
            GetFwdBwd(lowPy);
            GetCubeVec();
            return;
        }

        // matmul相关 api
        if (StringUtil::ContainAnyOfSubStr(lowPy, OVERALL_MATMUL_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_MATMUL);
            GetCubeVec();
            return;
        }

        // conv相关 api
        if (StringUtil::StartWith(lowPy, OVERALL_CONV_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_CONV);
            GetFwdBwdByTid(lowPy);
            GetCubeVec();
            return;
        }
    }

    void GetFwdBwd(const std::string &lowPy) {
        if (StringUtil::ContainAnyOfSubStr(lowPy, OVERALL_BWD_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_BACKWARD);
            return;
        }
        categoryList.emplace_back(OVERALL_CAT_FORWARD);
    }

    void GetFwdBwdByTid(const std::string &lowPy) {
        if (isInBwdTrack) {
            categoryList.emplace_back(OVERALL_CAT_BACKWARD);
            return;
        } else {
            GetFwdBwd(lowPy);
        }
    }

    void GetFwdBwdByOpType(const std::string &lowOpType) {
        if (StringUtil::ContainAnyOfSubStr(lowOpType, OVERALL_BWD_OP_MASK)) {
            categoryList.emplace_back(OVERALL_CAT_BACKWARD);
        } else {
            categoryList.emplace_back(OVERALL_CAT_FORWARD);
        }
    }

    void GetCubeVec() {
        if (aicoreTime > 0 || macTime > 0 || cubeTime > 0) {
            categoryList.emplace_back(OVERALL_CAT_CUBE);
        } else {
            categoryList.emplace_back(OVERALL_CAT_VECTOR);
        }
    }
};

class SystemViewOverallHelper {
  public:
    std::vector<CpuCubeOpInfo> cpuCubeOps;
    std::vector<OverallTmpInfo> kernelEvents;
    uint64_t bwdTrackId{};
    std::vector<OverallTmpInfo> overlapInfos;
    bool needResponse = false;
    double e2eTime{};
    // 每行结果返回一个唯一id
    uint32_t idCounter{};
    void CategorizeComputingEvents();
    std::vector<SameOperatorsDetails> FilterComputingEventsByCategory(
        const std::vector<std::string> &expectList, uint64_t minTimeStamp, const std::string &opName);
    void AggregateComputingOverallMetrics(std::vector<SystemViewOverallRes> &responseBody);

  private:
    // SummarizeOverall最大递归深度为5层，防止无穷递归
    const uint32_t maxDepth = 5;
    const int timeScale = 1000; // us转化为ns
    static void UpdateSystemViewResStatus(SystemViewOverallRes &currentRes, const OverallTmpInfo &tmpInfo);
    void SummarizeSystemViewOverall(SystemViewOverallRes &currentRes, uint32_t depth);
    void ComputeOverallMetrics(std::vector<SystemViewOverallRes> &resList, const OverallTmpInfo &tmpInfo, size_t index);
    SystemViewOverallRes &FindOrCreateChild(std::vector<SystemViewOverallRes> &list, const std::string &name);
};
}
#endif // PROFILER_SERVER_SYSTEMVIEWOVERALLHELPER_H
