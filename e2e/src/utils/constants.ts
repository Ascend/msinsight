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

import * as path from 'path';

// 动态获取当前文件路径（跨平台）
const getCurrentFilePath = (): string => {
    return __dirname;
};

const DATA_PATH = 'C:\\msinsight-quick-start-demo\\GUI-test-data';

export const FilePath = {
    TEXT: `${DATA_PATH}\\training\\single-node\\level2_text`,
    TEXT_RANK_0: `${DATA_PATH}\\training\\single-node\\level2_text\\rank_0_ascend_pt\\ASCEND_PROFILER_OUTPUT`,
    TEXT_RANK_2: `${DATA_PATH}\\training\\single-node\\level2_text\\rank_2_ascend_pt\\ASCEND_PROFILER_OUTPUT`,
    DB: `${DATA_PATH}\\training\\single-node\\level2`,
    DB_RANK_0: `${DATA_PATH}\\training\\single-node\\level2\\rank_0_ascend_pt\\ASCEND_PROFILER_OUTPUT`,
    DB_RANK_1: `${DATA_PATH}\\training\\single-node\\level2\\rank_1_ascend_pt\\ASCEND_PROFILER_OUTPUT`,
    OP_SIMULATOR_BIN: `${DATA_PATH}\\operator\\msprof-op-simulator\\visualize_data.bin`,
    OP_SIMULATOR_JSON: `${DATA_PATH}\\operator\\msprof-op-simulator\\trace.json`,
    MULTI_NODES: `${DATA_PATH}\\training\\multi-node\\MultiProfLevel2MemoryUB_db`,
    MULTI_NODES_NODE_0_RANK_0: `${DATA_PATH}\\training\\multi-node\\MultiProfLevel2MemoryUB_db\\node1_2166651_20240619060505060_ascend_pt\\ASCEND_PROFILER_OUTPUT`,
    MULTI_NODES_NODE_0_RANK_1: `${DATA_PATH}\\training\\multi-node\\MultiProfLevel2MemoryUB_db\\node1_2166652_20240619060505059_ascend_pt\\ASCEND_PROFILER_OUTPUT`,
    MULTI_NODES_NODE_1_RANK_0: `${DATA_PATH}\\training\\multi-node\\MultiProfLevel2MemoryUB_db\\ubuntu2204_1660963_20240619060440181_ascend_pt\\ASCEND_PROFILER_OUTPUT`,
    MULTI_NODES_NODE_1_RANK_1: `${DATA_PATH}\\training\\multi-node\\MultiProfLevel2MemoryUB_db\\ubuntu2204_1660964_20240619060440179_ascend_pt\\ASCEND_PROFILER_OUTPUT`,
    DETAILS: `${DATA_PATH}\\operator\\msprof-op\\details\\visualize_data.bin`,
    DETAILS_ROOFLINE: `${DATA_PATH}\\operator\\msprof-op\\roofline\\visualize_data.bin`,
    DETAILS_CORE: `${DATA_PATH}\\operator\\msprof-op\\core_inter_load\\visualize_data.bin`,
    REINFORCEMENT_LEARNING: `${DATA_PATH}\\training\\reinforcement-learning\\verl`,
    LEAKS_DUMP: `${DATA_PATH}\\memscope\\leaks_dump_20250806.db`,
    SNAPSHOT_PKL: `${DATA_PATH}\\memscope\\snapshot_expandable.pkl`,

    TEXT_330: 'D:\\GUI_TEST_DATA 路徑测试\\系统调优\\test_level1_PipeUtilization\\profiling_data', // 给 Memory 模块使用
    TEXT_330_RANK_0: 'D:\\GUI_TEST_DATA 路徑测试\\系统调优\\test_level1_PipeUtilization\\profiling_data\\ubuntu2204_3929556_20250411014455476_ascend_pt\\ASCEND_PROFILER_OUTPUT',
    MIND_SPORE: 'D:\\GUI_TEST_DATA 路徑测试\\mindspore\\profile',
    // 专家负载均衡数据
    MOE_PROFILING: 'D:\\GUI_TEST_DATA 路徑测试\\系统调优\\MoE_balancing\\profiling',
    MS_RANK_0: 'D:\\GUI_TEST_DATA 路徑测试\\mindspore\\profile\\test123_3028773_20250617091627998_ascend_ms',
    //联合冒烟数据
    JOINT_DATA: '/home/profiler_performance/task',

    // 预冒烟数据
    SMOKE_DATA: path.join(getCurrentFilePath(), '..', '..', '..', 'test', 'st', 'level2'),
};

export const WEBSOCKET_URL = 'ws://localhost:9000/';
