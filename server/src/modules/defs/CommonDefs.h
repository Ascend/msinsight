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

#ifndef PROFILER_SERVER_COMMONDEFS_H
#define PROFILER_SERVER_COMMONDEFS_H

#include <string>
#include <vector>

// clang-format off
namespace Dic {
    const static std::string MSPROF_PREFIX = "[MSPROF]";
    const static std::string MSPROF_CONNECT = "__";

    // PyTorch训练数据无分片且无时间戳，msprof离线推理数据一定有时间戳，可能有分片
    const std::string memoryOperatorReg =
        R"((operator_memory|operator_memory(_slice_[0-9]{1,2})?_[0-9]{1,14})\.csv$)";
    const std::string memoryRecordReg =
        R"((memory_record|memory_record(_slice_[0-9]{1,2})?_[0-9]{1,14})\.csv$)";
    const std::string staticOpMemReg =
        R"((static_op_mem|static_op_mem(_slice_[0-9]{1,2})?_[0-9]{1,14})\.csv$)";
    const std::string npuModuleMemReg =
        R"((npu_module_mem|npu_module_mem(_slice_[0-9]{1,2})?_[0-9]{1,14})\.csv$)";
    const static std::string KERNEL_DETAIL_REG =
        R"((kernel_details|op_summary(_slice_[0-9]{1,2})?_[0-9]{1,14})\.csv$)";
    const std::string traceViewReg =
    R"((((trace_view|msprof(_slice_[0-9]{1,2})?_[0-9]{1,14})\.json)|)"
    R"((operator_memory|operator_memory(_slice_[0-9]{1,2})?_[0-9]{1,14})\.csv)$)";
    const std::string clusterReg = R"(cluster_analysis_output$)";
    const std::string ftraceDbReg = R"(^ftrace_data[0-9]*\.db$)";
    const std::string DBReg =
            R"((msprof_[0-9]{1,16}|((ascend_pytorch_profiler)(_[0-9]{1,16}){0,1})|cluster_analysis)\.db$)";
    const std::string msprofDBReg = R"(msprof_[0-9]{1,16}\.db$)";
    const std::string platformDBReg = R"(platform\.db$)";
    const std::string pytorchDBReg = R"((ascend_pytorch_profiler)(_[0-9]{1,16}){0,1}\.db$)";
    const std::string memScopeDbReg = R"(^(memscope|leaks)_dump_\d+\.db$)";
    const std::string mindsporeDBReg = R"((ascend_mindspore_profiler)(_[0-9]{1,16}){0,1}\.db$)";
    const std::string memSnapshotDbReg = R"(\.(pkl|pickle).db$))";
    const std::string npumonitorDBReg = R"(msmonitor_\d+_\d+_(-1|\d+)\.db)";
    const std::string clusterDBReg = R"(cluster_analysis\.db$)";
    const std::string computeBinSuffix = ".bin";
    const std::string ipynbSuffix = ".ipynb";
    const std::string pickleSuffix = ".pickle";
    const std::string pickleAbbreviationSuffix = ".pkl";

    const static std::vector<std::string> CANN_APIS = {"acl", "runtime", "model", "node", "hccl", "communication"};
    const static std::vector<std::string> OVERLAP_TYPES = {"Computing", "Communication ",
                                                           "Communication(Not Overlapped)", "Free"};
    const static std::string E2E_TIME = "E2E Time";
    const static std::string COMPUTING_TIME = "Computing Time";
    const static std::string COMMUNICATION_NOT_OVERLAP_TIME = "Communication(Not Overlapped) Time";
    const static std::string COMMUNICATION_TIME = "Communication Time";
    const static std::string FREE_TIME = "Free Time";
    const static std::string WAIT_TIME = "Wait Time";
    const static std::string TRANSMIT_TIME = "Transmit Time";
}
// clang-format on

#endif // PROFILER_SERVER_COMMONDEFS_H
