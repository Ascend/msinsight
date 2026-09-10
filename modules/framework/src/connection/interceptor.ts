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
import type { NotificationInterceptor, ResponseInterceptor } from './defs';
import {
    type ImportActionResponse,
    parseMemorySuccessHandler,
    parseStatisticSuccessHandler,
    parseLeaksSuccessHandler,
    parseTritonSuccessHandler,
    parseOperatorSuccessHandler,
    profilingExpertDataParsedHandler,
    parseMemSnapshotSliceReadyHandler,
    parseMemSnapshotProgressHandler,
} from './interceptorHandler';

const MEMORY_COMPLETED = 'parse/memoryCompleted';
const STATISTIC_COMPLETED = 'parse/statisticCompleted';
const LEAKS_COMPLETED = 'parse/leaksMemoryCompleted';
const TRITON_COMPLETED = 'parse/tritonCompleted';
const OPERATOR_COMPLETED = 'parse/operatorCompleted';
const PARSE_HEATMAP_COMPLETED = 'parse/heatmapCompleted';
const MEM_SNAPSHOT_PROGRESS = 'parse/memSnapshotProgress';
const MEM_SNAPSHOT_SLICE_READY = 'parse/memSnapshotSliceReady';

export type ResponseType = ImportActionResponse;
export const INTERCEPTOR_HANDLERS: Record<string, ResponseInterceptor<ResponseType>> = {};

export const NOTIFICATION_INTERCEPTOR_HANDLERS: Record<string, NotificationInterceptor<any>> = {
    [MEMORY_COMPLETED]: parseMemorySuccessHandler,
    [LEAKS_COMPLETED]: parseLeaksSuccessHandler,
    [TRITON_COMPLETED]: parseTritonSuccessHandler,
    [OPERATOR_COMPLETED]: parseOperatorSuccessHandler,
    [PARSE_HEATMAP_COMPLETED]: profilingExpertDataParsedHandler,
    [MEM_SNAPSHOT_PROGRESS]: parseMemSnapshotProgressHandler,
    [MEM_SNAPSHOT_SLICE_READY]: parseMemSnapshotSliceReadyHandler,
};

export const NOTIFICATION_STATISTIC_HANDLERS: Record<string, NotificationInterceptor<any>> = {
    [STATISTIC_COMPLETED]: parseStatisticSuccessHandler,
};
