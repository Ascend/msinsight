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

#ifndef PROFILER_SERVERDEFS_H
#define PROFILER_SERVERDEFS_H

#include <string>

namespace Dic {
// table name
const static std::string TABLE_TIME_INFO = "communication_time_info";
const static std::string TABLE_BANDWIDTH = "communication_bandwidth_info";
const static std::string TABLE_STEP_TRACE = "step_statistic_info";
const static std::string TABLE_BASE_INFO = "cluster_base_info";
const static std::string TABLE_COMMUNICATION_MATRIX = "communication_matrix";
const static std::string TABLE_GROUP_ID = "group_id";
const static std::string TABLE_KERNEL = "kernel_detail";
const static std::string TABLE_STATIC_OPERATOR = "static_op";
const static std::string TABLE_DYNAMIC_OPERATOR = "operator";

// db table name
const static std::string TABLE_TASK = "TASK";
const static std::string TABLE_API = "PYTORCH_API";
const static std::string TABLE_PYTORCH_CALLCHAINS = "PYTORCH_CALLCHAINS";
const static std::string TABLE_CANN_API = "CANN_API";
const static std::string TABLE_OSRT_API = "OSRT_API";
const static std::string TABLE_MSTX_EVENTS = "MSTX_EVENTS";
const static std::string GC_RECORD = "GC_RECORD";
const static std::string TABLE_COMMUNICATION_SCHEDULE_TASK = "COMMUNICATION_SCHEDULE_TASK_INFO";
const static std::string TABLE_ENUM_MSTX_EVENTS = "ENUM_MSTX_EVENT_TYPE";
const static std::string TABLE_COMMUNICATION_TASK_INFO = "COMMUNICATION_TASK_INFO";
const static std::string TABLE_COMMUNICATION_OP = "COMMUNICATION_OP";
const static std::string TABLE_COMPUTE_TASK_INFO = "COMPUTE_TASK_INFO";
const static std::string TABLE_TASK_PMU_INFO = "TASK_PMU_INFO";
const static std::string TABLE_ENUM_API_LEVEL = "ENUM_API_LEVEL";
const static std::string TABLE_STRING_IDS = "STRING_IDS";
const static std::string TABLE_NPU_INFO = "NPU_INFO";
const static std::string TABLE_SESSION_TIME_INFO = "SESSION_TIME_INFO";
const static std::string TABLE_MEMORY_RECORD = "MEMORY_RECORD";
const static std::string TABLE_OPERATOR_MEMORY = "OP_MEMORY";
const static std::string TABLE_NPU_MODULE_MEM = "NPU_MODULE_MEM";
const static std::string TABLE_NPU_MEM = "NPU_MEM";
const static std::string TABLE_GE_MEMORY = "NPU_OP_MEM";
const static std::string TABLE_LEAKS_DUMP = "leaks_dump"; // 仍需兼容旧表名
const static std::string TABLE_MEM_SCOPE_DUMP = "memscope_dump"; // 适配msleaks更名memscope
const static std::string TABLE_PYTORCH_INFO = "RANK_DEVICE_MAP";
const static std::string TABLE_OVERLAP_ANALYSIS = "OVERLAP_ANALYSIS";
const static std::string TABLE_META_DATA = "META_DATA";
const static std::string TABLE_CONNECTION_IDS = "CONNECTION_IDS";
const static std::string TABLE_CONNECTION_CATS = "connectionCats";
const static std::string TABLE_ENUM_API_TYPE = "ENUM_API_TYPE";
const static std::string TABLE_HOST_INFO = "HOST_INFO";
const static std::string TABLE_MEMCPY_INFO = "MEMCPY_INFO";
const static std::string TABLE_ENUM_MEMCPY_OPERATION = "ENUM_MEMCPY_OPERATION";
const static std::string TABLE_ENUM_HCCL_TRANSPORT_TYPE = "ENUM_HCCL_TRANSPORT_TYPE";
const static std::string TABLE_ENUM_HCCL_LINK_TYPE = "ENUM_HCCL_LINK_TYPE";
const static std::string TABLE_STEP_TASK_INFO = "StepTaskInfo";

// cluster db table
const static std::string TABLE_STEP_TRACE_TIME = "ClusterStepTraceTime";
const static std::string TABLE_COMM_ANALYZER_TIME = "ClusterCommunicationTime";
const static std::string TABLE_COMM_ANALYZER_BANDWIDTH = "ClusterCommunicationBandwidth";
const static std::string TABLE_COMM_ANALYZER_MATRIX = "ClusterCommunicationMatrix";
const static std::string TABLE_COMM_GROUP = "CommunicationGroupMapping";
const static std::string TABLE_CLUSTER_BASE_INFO = "ClusterBaseInfo";

// 集群公共表（text和db共有）
const static std::string TABLE_EXPERT_HOTSPOT_INTO = "ExpertHotspotInfo";
const static std::string TABLE_EXPERT_DEPLOYMENT_INFO = "ExpertDeploymentInfo";

// database version
#ifndef DATABASE_VERSION
#define DATABASE_VERSION 0
#endif
} // end of namespace Dic

#endif // PROFILER_SERVERDEFS_H
