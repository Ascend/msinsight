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
import { selectedItemForConnection, selectedItemForNuma, selectedItemForSocket } from '@/features/topology/model/selection';
import { numaOverviewFixture } from '@/testUtils/numaOverview.fixture';

const t = ((key: string, options?: { count?: number }) => `${key}:${options?.count ?? ''}`) as TFunction<'numa'>;

describe('topology selection', () => {
    it('builds socket, NUMA, and connection selections from backend data', () => {
        const socket = numaOverviewFixture.sockets[0];
        const numa = socket.numas[0];
        const connection = numaOverviewFixture.connections[1];

        expect(selectedItemForSocket(socket, t)).toMatchObject({
            kind: 'socket', id: 'socket-0', title: 'Socket 0', description: 'diagram.socketDescription:2',
        });
        expect(selectedItemForNuma(numa, socket, t)).toMatchObject({
            kind: 'numa',
            id: 'numa-0',
            title: 'NUMA Node 0 / Socket 0',
            metrics: [numa.metrics[4], numa.metrics[2], numa.metrics[1], numa.metrics[0]],
        });
        expect(selectedItemForConnection(connection)).toEqual({
            kind: 'connection',
            id: connection.id,
            title: connection.label,
            description: connection.description,
            metrics: connection.metrics,
        });
    });

    it('omits unavailable NUMA detail metrics without changing order', () => {
        const socket = numaOverviewFixture.sockets[0];
        expect(selectedItemForNuma(socket.numas[1], socket, t).metrics.map(({ key }) => key)).toEqual([
            'crossSocketRead',
        ]);
    });
});
