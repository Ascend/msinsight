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

import type { TFunction } from 'i18next';
import type { Connection, SelectedItem, SocketNode } from '@/entities/numa/types';

const NUMA_DETAIL_METRIC_KEYS = [
    'llcTraffic',
    'innerRead',
    'totalRead',
    'crossSocketRead',
];

export function selectedItemForSocket(socket: SocketNode, t: TFunction<'numa'>): SelectedItem {
    return {
        kind: 'socket',
        id: `socket-${socket.id}`,
        title: socket.name,
        description: t('diagram.socketDescription', { count: socket.numas.length }),
        metrics: socket.metrics,
    };
}

export function selectedItemForNuma(
    numa: SocketNode['numas'][number],
    socket: SocketNode,
    t: TFunction<'numa'>,
): SelectedItem {
    const metrics = NUMA_DETAIL_METRIC_KEYS.flatMap(key => numa.metrics.filter(metric => metric.key === key));
    return {
        kind: 'numa',
        id: `numa-${numa.id}`,
        title: `${numa.name} / ${socket.name}`,
        description: t('diagram.numaDescription'),
        metrics,
    };
}

export function selectedItemForConnection(connection: Connection): SelectedItem {
    return {
        kind: 'connection',
        id: connection.id,
        title: connection.label,
        description: connection.description,
        metrics: connection.metrics,
    };
}
