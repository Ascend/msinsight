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

#ifndef PROFILER_SERVER_OPERATORPROTOCOLDEFS_H
#define PROFILER_SERVER_OPERATORPROTOCOLDEFS_H

#include "pch.h"

namespace Dic::Protocol {
// operator/statistic视图下的所有字段
namespace OperatorStatisticView {
// 计算算子类型 && 计算算子类型及输入Shape && 通信算子类型 公共字段
constexpr std::string_view OP_TYPE = "opType";
constexpr std::string_view COUNT = "count";
constexpr std::string_view TOTAL_TIME = "totalTime";
constexpr std::string_view AVG_TIME = "avgTime";
constexpr std::string_view MAX_TIME = "maxTime";
constexpr std::string_view MIN_TIME = "minTime";
// 计算算子类型 && 计算算子类型及输入Shape 公共字段
constexpr std::string_view ACC_CORE = "accCore";
// 仅计算算子类型及输入Shape
constexpr std::string_view OP_NAME = "opName";
constexpr std::string_view INPUT_SHAPE = "inputShape";

// 完整列
inline const std::set<std::string_view> FULL_COLS = {
    OP_TYPE, COUNT, TOTAL_TIME, AVG_TIME, MAX_TIME, MIN_TIME, ACC_CORE, OP_NAME, INPUT_SHAPE};
// 可排序列
inline const std::set<std::string_view> VALID_ORDER_COLS = FULL_COLS;
// 可筛选列
inline const std::set<std::string_view> VALID_FILTER_COLS = {OP_TYPE, OP_NAME, ACC_CORE, INPUT_SHAPE};
}
// operator/detail和operator/more_info下的所有字段(除PMU header外)
namespace OperatorDetailsView {
// 计算算子 && 通信算子公共字段
constexpr std::string_view NAME = "name";
constexpr std::string_view TYPE = "type";
constexpr std::string_view ACC_CORE = "accCore";
constexpr std::string_view START_TIME = "startTime";
constexpr std::string_view DURATION = "duration";
constexpr std::string_view WAIT_TIME = "waitTime";

// 仅计算算子
constexpr std::string_view BLOCK_NUM = "blockNum";
constexpr std::string_view INPUT_SHAPES = "inputShape";
constexpr std::string_view INPUT_DATA_TYPES = "inputType";
constexpr std::string_view INPUT_FORMATS = "inputFormat";
constexpr std::string_view OUTPUT_SHAPES = "outputShape";
constexpr std::string_view OUTPUT_DATA_TYPES = "outputType";
constexpr std::string_view OUTPUT_FORMATS = "outputFormat";

// 完整列
inline const std::set<std::string_view> FULL_COLS = {NAME, TYPE, ACC_CORE, START_TIME, DURATION, WAIT_TIME, BLOCK_NUM,
    INPUT_SHAPES, INPUT_DATA_TYPES, INPUT_FORMATS, OUTPUT_SHAPES, OUTPUT_DATA_TYPES, OUTPUT_FORMATS};
// 可排序列
inline const std::set<std::string_view> VALID_ORDER_COLS = FULL_COLS;
// 可筛选列
inline const std::set<std::string_view> VALID_FILTER_COLS = {NAME, TYPE, ACC_CORE};
}
}
#endif // PROFILER_SERVER_OPERATORPROTOCOLDEFS_H
