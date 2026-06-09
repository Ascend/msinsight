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

#ifndef PROFILER_SERVER_TEXTTABLECOLUM_H
#define PROFILER_SERVER_TEXTTABLECOLUM_H
#include <string>
namespace Dic {
namespace Module {
namespace Timeline {
#pragma region << Slice Table Info>>
namespace SliceColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view TIMESTAMP = "timestamp";
constexpr std::string_view DURATION = "duration";
constexpr std::string_view NAME = "name";
constexpr std::string_view TRACKID = "track_id";
constexpr std::string_view CAT = "cat";
constexpr std::string_view ARGS = "args";
constexpr std::string_view CNAME = "cname";
constexpr std::string_view ENDTIME = "end_time";
constexpr std::string_view FLAGID = "flag_id";
constexpr std::string_view GROUPID = "group_id";
}
#pragma endregion
namespace FlowColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view FLOW_ID = "flow_id";
constexpr std::string_view NAME = "name";
constexpr std::string_view CAT = "cat";
constexpr std::string_view TRACK_ID = "track_id";
constexpr std::string_view TIMESTAMP = "timestamp";
constexpr std::string_view TYPE = "type";
}

namespace ThreadColumn {
constexpr std::string_view TRACK_ID = "track_id";
constexpr std::string_view TID = "tid";
constexpr std::string_view PID = "pid";
constexpr std::string_view THREAD_NAME = "thread_name";
constexpr std::string_view THREAD_SORT_INDEX = "thread_sort_index";
}

namespace ProcessColumn {
constexpr std::string_view PID = "pid";
constexpr std::string_view PROCESS_NAME = "process_name";
constexpr std::string_view LABEL = "label";
constexpr std::string_view PROCESS_SORT_INDEX = "process_sort_index";
}

namespace CounterColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view NAME = "name";
constexpr std::string_view PID = "pid";
constexpr std::string_view TIMESTAMP = "timestamp";
constexpr std::string_view CAT = "cat";
constexpr std::string_view ARGS = "args";
}

namespace KernelDetailColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view RANK_ID = "deviceId";
constexpr std::string_view STEP_ID = "step_id";
constexpr std::string_view NAME = "name";
constexpr std::string_view OP_TYPE = "op_type";
constexpr std::string_view ACCELERATOR_CORE = "accelerator_core";
constexpr std::string_view START_TIME = "start_time";
constexpr std::string_view DURATION = "duration";
constexpr std::string_view WAIT_TIME = "wait_time";
constexpr std::string_view BLOCK_DIM = "block_dim";
constexpr std::string_view INPUT_SHAPES = "input_shapes";
constexpr std::string_view INPUT_DATA_TYPES = "input_data_types";
constexpr std::string_view INPUT_FORMATS = "input_formats";
constexpr std::string_view OUTPUT_SHAPES = "output_shapes";
constexpr std::string_view OUTPUT_DATA_TYPES = "output_data_types";
constexpr std::string_view OUTPUT_FORMATS = "output_formats";
}

namespace TaskColumn {
constexpr std::string_view ROW_ID = "rowid";
constexpr std::string_view TIMESTAMP = "startNs";
constexpr std::string_view ENDTIME = "endNs";
constexpr std::string_view DECICED_ID = "deviceId";
constexpr std::string_view CONNECTION_ID = "connectionId";
constexpr std::string_view GLOBAL_TASK_ID = "globalTaskId";
constexpr std::string_view GLOBAL_PID = "globalPid";
constexpr std::string_view TASK_TYPE = "taskType";
constexpr std::string_view CONTEXT_ID = "contextId";
constexpr std::string_view STREAM_ID = "streamId";
constexpr std::string_view TASK_ID = "taskId";
constexpr std::string_view MODEL_ID = "modelId";
}

namespace TaskPmuInfoColumn {
constexpr std::string_view GLOBAL_TASK_ID = "globalTaskId";
constexpr std::string_view NAME_ID = "name";
constexpr std::string_view VALUE_ID = "value";
}

namespace CommucationTaskInfoColumn {
constexpr std::string_view ROW_ID = "rowid";
constexpr std::string_view NAME = "name";
constexpr std::string_view GLOBAL_TASK_ID = "globalTaskId";
constexpr std::string_view TASK_TYPE = "taskType";
constexpr std::string_view PLANE_ID = "planeId";
constexpr std::string_view GROUPNAME = "groupName";
constexpr std::string_view NOTIFY_ID = "notifyId";
constexpr std::string_view RDMA_TYPE = "rdmaType";
constexpr std::string_view SRC_RANK = "srcRank";
constexpr std::string_view DST_RANK = "dstRank";
constexpr std::string_view TRANSPORT_TYPE = "transportType";
constexpr std::string_view SIZE = "size";
constexpr std::string_view DATA_TYPE = "dataType";
constexpr std::string_view LINK_TYPE = "linkType";
constexpr std::string_view BANDWIDTH = "bandwidth";
constexpr std::string_view OP_ID = "opId";
}

namespace CommucationTaskOpColumn {
constexpr std::string_view OP_NAME = "opName";
constexpr std::string_view TIMESTAMP = "startNs";
constexpr std::string_view ENDTIME = "endNs";
constexpr std::string_view CONNECTION_ID = "connectionId";
constexpr std::string_view GROUPNAME = "groupName";
constexpr std::string_view OP_ID = "opId";
constexpr std::string_view RELAY = "relay";
constexpr std::string_view RETRY = "retry";
constexpr std::string_view DATA_TYPE = "dataType";
constexpr std::string_view ALG_TYPE = "algType";
constexpr std::string_view COUNT = "count";
constexpr std::string_view OP_TYPE = "opType";
constexpr std::string_view WAIT_TIME = "waitNs";
}

namespace CANNApiColumn {
constexpr std::string_view ID = "connectionId";
constexpr std::string_view TIMESTAMP = "startNs";
constexpr std::string_view ENDTIME = "endNs";
constexpr std::string_view TYPE = "type";
// 该api所属的全局tid。高32位：pid，低32位：tid
constexpr std::string_view GLOBAL_TID = "globalTid";
constexpr std::string_view NAME = "name";
constexpr std::string_view DEPTH = "depth";
}

namespace ConnectionIdsColumn {
// 索引，对应PYTORCH_API表的connectionId
constexpr std::string_view ID = "id";
// 用于表示关联关系的id，当前包括task_queue、fwd_bwd、torch-cann-task三种关联关系
constexpr std::string_view CONNECTIONID = "connectionId";
}

namespace NpuInfoColumn {
// deviceId
constexpr std::string_view ID = "id";
// 芯片型号
constexpr std::string_view NAME = "name";
}

namespace PytorchApiColumn {
constexpr std::string_view ID = "rowid";
// op api开始时间
constexpr std::string_view TIMESTAMP = "startNs";
// op api结束时间
constexpr std::string_view ENDTIME = "endNs";
// 该api所属的全局tid。高32位：pid，低32位：tid
constexpr std::string_view GLOBAL_TID = "globalTid";
// 索引，用于在CONNECTION_IDS表查询对应的connection_id;如果无connection_id，此处为空
constexpr std::string_view CONNECTIONID = "connectionId";
// 该op api名在STRING_IDS表中对应的id
constexpr std::string_view NAME = "name";
// 	op序号
constexpr std::string_view SEQUENCE_NUMBER = "sequenceNumber";
// op前向线程id
constexpr std::string_view FWD_THREAD_ID = "fwdThreadId";
// 输入数据类型在STRING_IDS表中对应的id
constexpr std::string_view INPUT_DTYPES = "inputDtypes";
// 输入shape在STRING_IDS表中对应的id
constexpr std::string_view INPUT_SHAPES = "inputShapes";
// 索引，用于在PYTORCH_CALLCHAINS表查询对应的call stack信息;如果无stack信息，此处为空
constexpr std::string_view CALL_CHAIN_ID = "callchainId";
// 标记数据类型，op、queue、mstx还是python_trace，数据类型存于枚举表ENUM _API_TYPE中
constexpr std::string_view TYPE = "type";
// depth
constexpr std::string_view DEPTH = "depth";
}

namespace StringIdsColumn {
// string 对应的id
constexpr std::string_view ID = "id";
// string内容
constexpr std::string_view VALUE = "value";
}

namespace HostInfoColumn {
constexpr std::string_view HOST_UID = "hostUid";
constexpr std::string_view HOST_NAME = "hostName";
}

namespace MstxEventsColumn {
constexpr std::string_view ID = "rowid";
//    host侧tx打点数据开始时间
constexpr std::string_view TIMESTAMP = "startNs";
//    host侧tx打点数据结束时间
constexpr std::string_view ENDTIME = "endNs";
//    host侧tx打点数据类型在ENUM_MSTX_EVENT_TYPE表里对应的id
constexpr std::string_view EVENT_TYPE = "eventType";
//    host侧range类型tx数据对应的range id(预留)
constexpr std::string_view RANG_ID = "rangeId";
//    host侧tx数据所属的分类id(预留)
constexpr std::string_view CATEGORY = "category";
//    host侧tx打点数据携带信息在STRING_IDS表里对应的id
constexpr std::string_view MESSAGE = "message";
//    host侧tx打点数据开始线程的全局tid
constexpr std::string_view GLOBAL_TID = "globalTid";
//    host侧tx打点数据结束线程的全局tid
constexpr std::string_view END_GLOBAL_TID = "endGlobalTid";
//    host侧tx打点数据所属域的域id(预留)
constexpr std::string_view DOMAIN_ID = "domainId";
//    host侧tx打点数据与TASK表里npu打点task的关联id
constexpr std::string_view CONNECTION_ID = "connectionId";
constexpr std::string_view DEPTH = "depth";
}

namespace PythonGCColumn {
constexpr std::string_view ID = "rowid";
//    host侧tx打点数据开始时间
constexpr std::string_view TIMESTAMP = "startNs";
//    host侧tx打点数据结束时间
constexpr std::string_view ENDTIME = "endNs";
}

namespace EnumHcclDataTypeClumn {
constexpr std::string_view ID = "id";
// hccl data type类型
constexpr std::string_view NAME = "name";
}

namespace EnumHcclRdmaTypeClumn {
constexpr std::string_view ID = "id";
// hccl rdma type类型
constexpr std::string_view NAME = "name";
}

namespace EnumHcclTransportTypeClumn {
constexpr std::string_view ID = "id";
// hccl transport type类型
constexpr std::string_view NAME = "name";
}

namespace EnumHcclLinkTypeClumn {
constexpr std::string_view ID = "id";
// hccl link type类型
constexpr std::string_view NAME = "name";
}

namespace ComputeTaskInfoColumn {
// 算子名，STRING_IDS(name)
constexpr std::string_view NAME = "name";
// 全局算子任务id，用于关联TASK表,主键
constexpr std::string_view GLOBAL_TASK_ID = "globalTaskId";
// 算子运行切分数量，对应算子运行时核数
constexpr std::string_view BLOCK_NUM = "blockNum";
constexpr std::string_view GRID_DIM = "gridDim";
constexpr std::string_view BLOCK_DIM = "blockDim";
// mix算子从加速器的block_dim值
constexpr std::string_view MIX_BLOCK_DIM = "mixBlockDim";
// host执行该算子的加速器类型，STRING_IDS(taskType)
constexpr std::string_view TASK_TYPE = "taskType";
// 算子类型，STRING_IDS(opType)
constexpr std::string_view OP_TYPE = "opType";
// 算子输入数据格式，STRING_IDS(inputFormats)
constexpr std::string_view INPUT_FORMATS = "inputFormats";
// 算子输入数据类型，STRING_IDS(inputDataTypes)
constexpr std::string_view INPUT_DATA_TYPES = "inputDataTypes";
// 算子的输入维度，STRING_IDS(inputShapes)
constexpr std::string_view INPUT_SHAPES = "inputShapes";
// 算子输出数据格式，STRING_IDS(outputFormats)
constexpr std::string_view OUTOUT_FORMATS = "outputFormats";
// 算子输出数据类型，STRING_IDS(outputDataTypes)
constexpr std::string_view OUTPUT_DATA_TYPES = "outputDataTypes";
// 算子输出维度，STRING_IDS(outputShapes)
constexpr std::string_view OUTPUT_SHAPES = "outputShapes";
// 算子的attr信息，用来映射算子shape，算子自定义的参数等，STRING_IDS(attrInfo)
constexpr std::string_view ATTRINFO = "attrInfo";
constexpr std::string_view WAIT_NS = "waitNs";
}

namespace EnumApiTypeColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view NAME = "name";
}

namespace EnumMstxEventTypeColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view NAME = "name";
}

namespace PytorchCallchainsColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view STACK = "stack";
constexpr std::string_view STACK_DEPTH = "stackDepth";
}
}
}
}
#endif // PROFILER_SERVER_TEXTTABLECOLUM_H
