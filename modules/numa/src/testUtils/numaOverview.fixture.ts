/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

import type { Metric, NumaOverview, NumaNode, SocketNode } from '@/entities/numa/types';

export const createMetric = (key: string, value: number, unit = 'GOps'): Metric =>
    ({ key, label: key, description: key, unit, value });

const numa = (id: number): NumaNode => ({
    id,
    name: `NUMA Node ${id}`,
    metrics: id === 0
        ? [
            createMetric('crossSocketRead', 1),
            { ...createMetric('totalRead', 4), label: 'Vendor Total Read Traffic' },
            createMetric('innerRead', 3),
            createMetric('dramRead', 2, 'GB'),
            createMetric('llcTraffic', 5, 'GB'),
        ]
        : [
            createMetric('crossSocketRead', 1),
            createMetric('dramRead', 2, 'GB'),
        ],
});
const socket = (id: number): SocketNode => ({
    id,
    name: `Socket ${id}`,
    metrics: [createMetric('crossSocketRead', 2)],
    numas: [numa(id * 2), numa(id * 2 + 1)],
});
export const numaOverviewFixture: NumaOverview = {
    range: { startTime: 1, endTime: 2 },
    totalMetrics: [createMetric('totalCrossSocketRead', 4)],
    sockets: [socket(0), socket(1)],
    connections: [
        {
            id: 'socket-link-0-1',
            type: 'socket',
            source: 'socket-0',
            target: 'socket-1',
            label: 'Socket 0 <-> Socket 1',
            description: 'socket traffic',
            metrics: [createMetric('sourceToTarget', 1)],
        },
        {
            id: 'numa-link-0-1',
            type: 'numa',
            source: 'numa-0',
            target: 'numa-1',
            label: 'NUMA Node 0 <-> NUMA Node 1',
            description: 'NUMA traffic',
            metrics: [{ ...createMetric('crossScclRead', 1), label: 'Cross SCCL Read Traffic' }],
        },
        ...[0, 1, 2, 3].map((id) => ({
            id: `memory-${id}`,
            type: 'memory' as const,
            source: `numa-${id}`,
            target: `memory-${id}`,
            label: `NUMA Node ${id} <-> DRAM`,
            description: 'DRAM traffic',
            metrics: [createMetric('dramRead', 2, 'GB')],
        })),
    ],
};
