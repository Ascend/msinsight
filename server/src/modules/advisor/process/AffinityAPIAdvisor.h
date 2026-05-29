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

#ifndef PROFILER_SERVER_AFFINITYAPIADVISOR_H
#define PROFILER_SERVER_AFFINITYAPIADVISOR_H

#include <map>
#include <set>
#include <vector>
#include "TimelineProtocolResponse.h"
#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"

namespace Dic::Module::Advisor {
const std::string API_SEP = "::";
struct AffinityApiData {
    std::vector<std::string> apiList;
    std::string affinityApi;
    std::string note;
};

const std::vector<AffinityApiData> AFFINITY_API_RULE = {
    {{"aten::gelu"}, "torch_npu.fast_gelu", ""},
    {{"aten::linear"}, "torch_npu.npu_linear", ""},
    {{"aten::mish"}, "torch_npu.npu_mish / torch_npu.contrib.module.Mish", ""},
    {{"aten::silu"}, "torch_npu.npu_silu / torch_npu.contrib.module.SiLU", ""},
    {{"aten::mul", "aten::sigmoid"}, "torch_npu.npu_silu / torch_npu.contrib.module.SiLU", ""},
    {{"aten::sigmoid", "aten::mul"}, "torch_npu.npu_silu / torch_npu.contrib.module.SiLU", ""},
    {{"aten::add", "aten::reciprocal", "aten::mul"}, "optimizer.clip_grad_norm_fused_", ""},
    // softmax-(mul){0,1}-(masked_fill_|add)
    {{"aten::softmax", "aten::masked_fill_|aten::add"}, "torch_npu.npu_scaled_masked_softmax", ""},
    {{"aten::softmax", "aten::mul", "aten::masked_fill_|aten::add"}, "torch_npu.npu_scaled_masked_softmax", ""},
    // "(permute|transpose)-(contiguous){0,1}-(reshape|view)", "(reshape|view)-(contiguous){0,1}-(permute|transpose)"
    {{"aten::permute|aten::transpose", "aten::reshape|aten::view"}, "torch_npu.npu_confusion_transpose", ""},
    {{"aten::permute|aten::transpose", "aten::contiguous", "aten::reshape|aten::view"},
        "torch_npu.npu_confusion_transpose", ""},
    {{"aten::reshape|aten::view", "aten::permute|aten::transpose"}, "torch_npu.npu_confusion_transpose", ""},
    {{"aten::reshape|aten::view", "aten::contiguous", "aten::permute|aten::transpose"},
        "torch_npu.npu_confusion_transpose", ""},

    // (chunk|slice)-neg-cat-(mul){0,2}-add
    {{"aten::chunk|aten::slice", "aten::neg", "aten::cat", "aten::add"}, "torch_npu.npu_rotary_mul", "CANN 7.0+"},
    {{"aten::chunk|aten::slice", "aten::neg", "aten::cat", "aten::mul", "aten::add"}, "torch_npu.npu_rotary_mul",
        "CANN 7.0+"},
    {{"aten::chunk|aten::slice", "aten::neg", "aten::cat", "aten::mul", "aten::mul", "aten::add"},
        "torch_npu.npu_rotary_mul", "CANN 7.0+"},
    // "matmul-(add){0,1}-(mul){0,1}-(masked_fill_|add){0,1}-softmax-(dropout){0,1}-matmul"
    {{"aten::matmul", "aten::softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    {{"aten::matmul", "aten::add", "aten::softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    {{"aten::matmul", "aten::mul", "aten::softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    {{"aten::matmul", "aten::add", "aten::mul", "aten::softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention",
        "CANN 7.0+"},
    {{"aten::matmul", "aten::masked_fill_softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    {{"aten::matmul", "aten::add", "aten::masked_fill_softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention",
        "CANN 7.0+"},
    {{"aten::matmul", "aten::mul", "aten::masked_fill_softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention",
        "CANN 7.0+"},
    {{"aten::matmul", "aten::add", "aten::mul", "aten::masked_fill_softmax", "aten::matmul"},
        "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    {{"aten::matmul", "dropout", "aten::softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    {{"aten::matmul", "aten::add", "dropout", "aten::softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention",
        "CANN 7.0+"},
    {{"aten::matmul", "aten::mul", "dropout", "aten::softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention",
        "CANN 7.0+"},
    {{"aten::matmul", "aten::add", "aten::mul", "dropout", "aten::softmax", "aten::matmul"},
        "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    {{"aten::matmul", "aten::masked_fill_dropout", "aten::softmax", "aten::matmul"}, "torch_npu.npu_fusion_attention",
        "CANN 7.0+"},
    {{"aten::matmul", "aten::add", "aten::masked_fill_dropout", "aten::softmax", "aten::matmul"},
        "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    {{"aten::matmul", "aten::mul", "aten::masked_fill_dropout", "aten::softmax", "aten::matmul"},
        "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    {{"aten::matmul", "aten::add", "aten::mul", "aten::masked_fill_dropout", "aten::softmax", "aten::matmul"},
        "torch_npu.npu_fusion_attention", "CANN 7.0+"},
    // (pow){0,1}-(mean){0,1}-(add){0,1}-rsqrt-mul-(type_as){0,1}
    {{"aten::rsqrt", "aten::mul"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::pow", "aten::rsqrt", "aten::mul"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::mean", "aten::rsqrt", "aten::mul"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::pow", "aten::mean", "aten::rsqrt", "aten::mul"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::add", "aten::rsqrt", "aten::mul"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::pow", "aten::add", "aten::rsqrt", "aten::mul"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::mean", "aten::add", "aten::rsqrt", "aten::mul"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::pow", "aten::mean", "aten::add", "aten::rsqrt", "aten::mul"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::rsqrt", "aten::mul", "aten::type_as"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::pow", "aten::rsqrt", "aten::mul", "aten::type_as"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::mean", "aten::rsqrt", "aten::mul", "aten::type_as"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::pow", "aten::mean", "aten::rsqrt", "aten::mul", "aten::type_as"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::add", "aten::rsqrt", "aten::mul", "aten::type_as"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::pow", "aten::add", "aten::rsqrt", "aten::mul", "aten::type_as"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::mean", "aten::add", "aten::rsqrt", "aten::mul", "aten::type_as"}, "torch_npu.npu_rms_norm", "CANN 7.0+"},
    {{"aten::pow", "aten::mean", "aten::add", "aten::rsqrt", "aten::mul", "aten::type_as"}, "torch_npu.npu_rms_norm",
        "CANN 7.0+"},

    /* "(slice|chunk)-silu-mul", "(slice|chunk)-mul-silu", "(slice|chunk)-sigmoid-mul-mul",
     "(slice|chunk)-mul-sigmoid-mul", "(slice|chunk)-mul-mul-sigmoid" */
    {{"aten::slice|aten::chunk", "aten::silu", "aten::mul"}, "torch_npu.npu_swiglu", "CANN 7.0+"},
    {{"aten::slice|aten::chunk", "aten::mul", "aten::silu"}, "torch_npu.npu_swiglu", "CANN 7.0+"},
    {{"aten::slice|aten::chunk", "aten::sigmoid", "aten::mul", "aten::mul"}, "torch_npu.npu_swiglu", "CANN 7.0+"},
    {{"aten::slice|aten::chunk", "aten::mul", "aten::sigmoid", "aten::mul"}, "torch_npu.npu_swiglu", "CANN 7.0+"},
    {{"aten::slice|aten::chunk", "aten::mul", "aten::mul", "aten::sigmoid"}, "torch_npu.npu_swiglu", "CANN 7.0+"},
    // (slice|chunk)-gelu-mul, (slice|chunk)-mul-gelu
    {{"aten::slice|aten::chunk", "aten::gelu", "aten::mul"}, "torch_npu.npu_geglu", "CANN 8.0+"},
    {{"aten::slice|aten::chunk", "aten::mul", "aten::gelu"}, "torch_npu.npu_geglu", "CANN 8.0+"},
};

const std::vector<std::string> AFFINITY_API_ORDER_BY_NAME_LIST = {"startTime", "duration", "pid", "tid", "name"};

class AffinityAPIAdvisor {
  public:
    static bool Process(const Protocol::APITypeParams &params, Protocol::AffinityAPIResBody &resBody);

  private:
    static std::set<std::string> GetFirstApiList(const std::vector<AffinityApiData> &affinityApiData);
    static std::vector<uint32_t> FilterPossibleRules(const std::string &name);
    static void FilterAffinityApiData(const Protocol::APITypeParams &params,
        std::vector<Protocol::FlowLocation> &dataList, const std::vector<uint32_t> &indexList,
        std::vector<Protocol::FlowLocation> &result);
    static bool CheckApiSeqWithRule(
        const std::vector<std::string> &rule, const std::vector<Protocol::FlowLocation> &dataList, uint32_t index);
    static std::vector<Protocol::FlowLocation> GetFlowLocationData(const Protocol::APITypeParams &params);
};
} // Dic::Module::Advisor

#endif // PROFILER_SERVER_AFFINITYAPIADVISOR_H
