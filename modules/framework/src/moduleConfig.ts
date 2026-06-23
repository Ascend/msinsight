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
import type { IframeHTMLAttributes } from 'react';

export interface ModuleConfig {
    name: string;
    requestName: Lowercase<string>;
    attributes: IframeHTMLAttributes<HTMLIFrameElement>;
    isDefault?: boolean;
    isCluster?: boolean;
    isCompute?: boolean;
    isLeaks?: boolean;
    isTriton?: boolean;
    isIE?: boolean;
    isRL?: boolean;
    hasCachelineRecords?: boolean;
    isOnlyTraceJson?: boolean;
    isHybridParse?: boolean;
}

const isDev = process.env.REACT_APP_ENV === 'development';
export const MEM_SCOPE_MODULE_NAME = 'MemScope';
export const ON_CHIP_MEMORY_MODULE_NAME = 'On-Chip Memory'; // 原 Triton
export const modulesConfig: ModuleConfig[] = [
    {
        name: 'Timeline',
        requestName: 'timeline',
        attributes: {
            src: isDev ? 'http://localhost:3000/' : './plugins/Timeline/index.html',
        },
        isDefault: true,
        isCluster: true,
        isCompute: true,
        isOnlyTraceJson: true,
        isIE: true,
        isLeaks: true,
        isTriton: true,
        isHybridParse: true,
    },
    {
        name: 'Memory',
        requestName: 'memory',
        attributes: {
            src: isDev ? 'http://localhost:3001/' : './plugins/Memory/index.html',
        },
        isDefault: true,
        isCluster: true,
        isHybridParse: true,
    },
    {
        name: 'Operator',
        requestName: 'operator',
        attributes: {
            src: isDev ? 'http://localhost:3002/' : './plugins/Operator/index.html',
        },
        isDefault: true,
        isCluster: true,
        isHybridParse: true,
    },
    {
        name: 'Summary',
        requestName: 'summary',
        attributes: {
            src: isDev ? 'http://localhost:3003/summary.html' : './plugins/Cluster/summary.html',
        },
        isCluster: true,
        isHybridParse: true,
    },
    {
        name: 'Communication',
        requestName: 'communication',
        attributes: {
            src: isDev ? 'http://localhost:3003/communication.html' : './plugins/Cluster/communication.html',
        },
        isCluster: true,
        isHybridParse: true,
    },
    {
        name: 'Source',
        requestName: 'compute',
        attributes: {
            src: isDev ? 'http://localhost:3004/source.html' : './plugins/Compute/source.html',
        },
        isCompute: true,
    },
    {
        name: 'Details',
        requestName: 'compute',
        attributes: {
            src: isDev ? 'http://localhost:3004/detail.html' : './plugins/Compute/detail.html',
        },
        isCompute: true,
    },
    {
        name: 'CacheFrame', // 注意：这里不能取名为 Cache，否则无法获取到该页面对象，导致通信有问题。因为 Cache 是 Window 上的关键字
        requestName: 'compute',
        attributes: {
            src: isDev ? 'http://localhost:3004/cache.html' : './plugins/Compute/cache.html',
        },
        hasCachelineRecords: true,
    },
    {

        name: 'Statistic',
        requestName: 'statistic',
        attributes: {
            src: isDev ? 'http://localhost:3006/' : './plugins/Statistic/index.html',
        },
        isIE: true,
        isHybridParse: true,
    },
    {
        name: MEM_SCOPE_MODULE_NAME,
        requestName: 'leaks',
        attributes: {
            src: isDev ? 'http://localhost:3007/' : './plugins/Leaks/index.html',
        },
        isLeaks: true,
    },
    {
        name: 'RL',
        requestName: 'rl',
        attributes: {
            src: isDev ? 'http://localhost:3008/' : './plugins/RL/index.html',
        },
        isRL: true,
    },
    {
        name: ON_CHIP_MEMORY_MODULE_NAME,
        requestName: 'triton',
        attributes: {
            src: isDev ? 'http://localhost:3009/' : './plugins/MemoryOnChip/index.html',
        },
        isTriton: true,
    },
];

export const ACP_SESSION_SRC = isDev ? 'http://localhost:3010' : './plugins/InsightWebAgent/index.html';
