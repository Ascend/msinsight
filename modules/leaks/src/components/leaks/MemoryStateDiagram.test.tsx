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
import React from 'react';
import { act, render } from '@testing-library/react';
import { MemoryStateDiagram } from './MemoryStateDiagram';
import { getSnapshotEvent } from '@/utils/RequestUtils';

jest.mock('@/utils/RequestUtils', () => ({
    getSnapshotEvent: jest.fn(),
    getMemoryStateData: jest.fn().mockResolvedValue({ segments: [] }),
}), { virtual: true });
jest.mock('@/utils/utils', () => ({ formatBytes: String }), { virtual: true });
jest.mock('@/leaksWorker/stateWorker/worker', () => ({
    workerInitCanvas: jest.fn(),
    workerResizeCanvas: jest.fn(),
    workerTransform: jest.fn(),
    workerHoverItem: jest.fn(),
    workerClickItem: jest.fn(),
    workerSetMemoryStateData: jest.fn(),
    workerSelectItem: jest.fn(),
}), { virtual: true });
jest.mock('@/leaksWorker/blockWorker/worker', () => ({ workerSelectItem: jest.fn() }), { virtual: true });
jest.mock('@insight/lib', () => ({
    Progress: () => null,
    ResizeTable: jest.requireActual('react').forwardRef(() => null),
}), { virtual: true });
jest.mock('@insight/lib/components', () => ({ Tooltip: () => null }), { virtual: true });
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('antd', () => ({ message: { warning: jest.fn() } }));
jest.mock('@ant-design/icons', () => ({
    BorderOutlined: () => null,
    LineChartOutlined: () => null,
    FlagOutlined: () => null,
    ArrowDownOutlined: () => null,
    ArrowUpOutlined: () => null,
    CloseCircleFilled: () => null,
    ColumnWidthOutlined: () => null,
    OneToOneOutlined: () => null,
    SearchOutlined: () => null,
}));
jest.mock('./tools', () => ({
    graphToolbarTooltipClassName: '',
    GraphKeycap: () => null,
    GraphMouseIcon: () => null,
    GraphShortcutActions: () => null,
    GraphShortcutRow: () => null,
    GraphShortcutTip: () => null,
    GraphShortcutTitle: () => null,
    GraphToolbar: () => null,
    GraphToolbarTooltipStyle: () => null,
    GraphWheelCombo: () => null,
    GraphWheelIcon: () => null,
    Loading: () => null,
    StateHoverItem: () => null,
}));

describe('snapshot event pagination during project switching', () => {
    it.each(['reset', 'unmount'])('stops requesting pages after %s', async transition => {
        let resolvePage: (value: any) => void = () => undefined;
        const request = getSnapshotEvent as jest.Mock;
        request.mockReset();
        request.mockReturnValueOnce(new Promise(resolve => { resolvePage = resolve; }));
        request.mockResolvedValue({ events: [], total: 0 });
        const session: any = {
            module: 'memsnapshot',
            deviceId: '0',
            deviceIds: { 0: ['BLOCK'] },
            fileHash: 'snapshot-a',
            eventType: 'BLOCK',
            selectedSliceIndex: 0,
            loadedMemoryBlockContextKey: JSON.stringify(['snapshot-a', 'memsnapshot', '0', 'BLOCK', 0]),
            pendingEventLocate: null,
            leaksWorkerInfo: { clickItem: null },
            stateWorkerInfo: {
                renderOptions: {
                    viewport: { width: 0, height: 0 },
                    transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 },
                    zoom: { x: 1, y: 1, offset: 0 },
                },
            },
        };
        const view = render(<MemoryStateDiagram session={session} />);
        expect(request).toHaveBeenCalledTimes(1);
        if (transition === 'unmount') {
            view.unmount();
        } else {
            // Reset is synchronous; an old response may arrive before React effects rerun.
            session.deviceId = '';
            session.module = 'leaks';
        }
        await act(async () => {
            resolvePage({ events: [{ id: 1, action: 'alloc' }], total: 2000 });
        });
        expect(request).toHaveBeenCalledTimes(1);
    });
});
