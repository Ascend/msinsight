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

#ifndef PROFILER_SERVER_SOURCEPROTOCOLTEST_H
#define PROFILER_SERVER_SOURCEPROTOCOLTEST_H

#include <string>

const std::string JSON = R"()";
const std::string TO_INTER_CORE_LOAD_GRAPH_JSON = R"(
{
  "id": 288,
  "moduleName": "source",
  "type": "request",
  "command": "source/details/interCoreLoadAnalysis",
  "params": {
  }
}
)";
const std::string TO_MEMORY_TABLE_REQ_JSON = R"(
{
  "id": 288,
  "moduleName": "source",
  "type": "request",
  "command": "source/details/memoryTable",
  "params": {
    "blockId": "0",
    "showAs": "request"
  }
}
)";
const std::string TO_MEMORY_GRAPH_REQ_JSON = R"(
{
  "id": 287,
  "moduleName": "source",
  "type": "request",
  "command": "source/details/memoryGraph",
  "params": {
    "blockId": "0",
    "showAs": "request"
  }
}
)";
const std::string TO_LOAD_INFO_REQ_JSON = R"(
{
  "id": 286,
  "moduleName": "source",
  "type": "request",
  "command": "source/details/computeworkload",
  "params": {
  }
}
)";
const std::string TO_BASE_INFO_REQ_JSON = R"(
{
  "id": 281,
  "moduleName": "source",
  "type": "request",
  "command": "source/details/baseInfo",
  "params": {
  }
}
)";
const std::string TO_API_INSTR_REQ_JSON = R"(
{
  "id": 4777,
  "moduleName": "source",
  "type": "request",
  "command": "source/api/instructions",
  "params": {
  }
}
)";
const std::string TO_API_LINE_REQ_JSON = R"(
{
  "id": 4776,
  "moduleName": "source",
  "type": "request",
  "command": "source/api/line",
  "params": {
    "sourceName": "xxx.cpp",
    "coreName": "core0.cubecore0"
  }
}
)";
const std::string TO_CODE_FILE_REQ_JSON = R"(
{
  "id": 4772,
  "moduleName": "source",
  "type": "request",
  "command": "source/code/file",
  "params": {
    "sourceName": "/home/xxx.cpp"
  }
}
)";
const std::string TO_TOP_WARP_STALL_REASON_REQ_JSON = R"(
{
  "id": 4773,
  "moduleName": "source",
  "type": "request",
  "command": "source/queryTopWarpStallReason",
  "params": {
  }
}
)";

#endif // PROFILER_SERVER_SOURCEPROTOCOLTEST_H
