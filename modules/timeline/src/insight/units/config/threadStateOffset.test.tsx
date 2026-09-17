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
// eslint-disable-next-line import/named
import { fireEvent, render, screen } from '@testing-library/react';
import type { CardMetaData, CounterMetaData, ThreadMetaData } from '../../../entity/data';
import type { ChartDesc, InsightUnit } from '../../../entity/insight';
import { Session } from '../../../entity/session';
import { ThreadingStateUnit, ThreadUnit } from '../AscendUnit';
import { CardOffsetConfig } from './offsetConfig';

jest.mock('react-i18next', () => ({
    useTranslation: (): { t: (key: string) => string } => ({
        t: (key: string): string => key,
    }),
}));
jest.mock('@emotion/react', () => ({
    useTheme: (): Record<string, any> => ({ buttonColor: {} }),
}));
jest.mock('@insight/lib/resize', () => ({
    ResizeTable: (): null => null,
    fetchColumnFilterProps: (): Record<string, never> => ({}),
}), { virtual: true });
jest.mock('../../../components/SelectedDataBottomPanel', () => ({
    SelectedDataBottomPanel: (): null => null,
}));
jest.mock('../../../components/details/SelectSimpleDetail', () => ({
    SelectSimpleTabularDetail: (): null => null,
}));
jest.mock('../../../components/details/utils', () => ({ renderRadiusBorder: jest.fn() }));
jest.mock('../../../components/ChartContainer/unitPin', () => ({
    isPinned: (): boolean => false,
    isSonPinned: (): boolean => false,
}));
jest.mock('../../../components/detailViews/Common', () => ({
    getDefaultColumData: (): Record<string, never> => ({}),
    getPageData: (): Record<string, never> => ({}),
    PageType: {},
}));
jest.mock('../details', () => ({
    generateFlowParam: (): Record<string, never> => ({}),
    slicesListDetail: {},
}));
jest.mock('../../../utils/jumpToUnitOperator', () => jest.fn());
jest.mock('../../../utils/operatorUnit', () => ({ findOperatorUnit: jest.fn() }));
jest.mock('../../../api/request', () => ({
    getUnitFlows: jest.fn(),
    queryAllSameOperatorsDuration: jest.fn(),
}));
jest.mock('../../../connection', () => ({
    __esModule: true,
    default: { addListener: jest.fn() },
}));
jest.mock('../counterUnit', () => ({ getCounterLaneDisplayName: jest.fn() }));

const CARD_ID = 'rank0';
const RAW_TIMESTAMP = 1_000_000_000;
const SESSION_END = 2_000_000_000;
const HOST_OFFSET = 250_000_000;
const DEVICE_OFFSET = 600_000_000;
const dataSource = { remote: 'local' } as unknown as DataSource;
const cardMetadata = { cardId: CARD_ID } as CardMetaData;
const hostThreadMetadata = {
    cardId: CARD_ID,
    dbPath: 'profile.db',
    dataSource,
    processId: '3043836',
    processName: 'Process 3043836',
    threadId: '3043836',
    threadName: 'PyTorch',
    metaType: 'PYTORCH_API',
} as ThreadMetaData;
const stateMetadata = {
    ...hostThreadMetadata,
    threadName: 'Thread State',
    metaType: 'THREADING_ANALYSIS',
    metricGroup: 'thread_state',
    bucketWidthNs: 500_000_000,
    dataType: ['Active', 'Sync Wait', 'Preemption', 'Unknown'],
} as CounterMetaData;

const createSession = (): Session => {
    const cardUnit = { metadata: cardMetadata } as unknown as InsightUnit;
    const currentSession = new Session({
        id: 'thread-state-offset-test',
        name: 'thread-state-offset-test',
        phase: 'configuring',
        units: [cardUnit],
        availableUnits: [],
        startRecordTime: 0,
        endTimeAll: SESSION_END,
        isNsMode: true,
    });
    currentSession.selectedUnits = [cardUnit];
    currentSession.unitsConfig.offsetConfig.timestampOffset = {
        [`${CARD_ID}__host`]: 0,
        [`${CARD_ID}__device`]: 0,
    };
    return currentSession;
};

interface TimelineRequest {
    command: string;
    params: Record<string, unknown>;
}

const createRequestMock = (): jest.Mock => jest.fn(async (_source: DataSource, request: TimelineRequest) => {
    if (request.command === 'unit/threadTraces') {
        return {
            data: [[{
                startTime: RAW_TIMESTAMP,
                duration: 100_000_000,
                name: 'ProfilerStep#3',
                cname: 'ProfilerStep#3',
                depth: 0,
                threadId: hostThreadMetadata.threadId,
                id: 'host-event',
            }]],
            maxDepth: 1,
            currentMaxDepth: 1,
            havePythonFunction: false,
        };
    }
    return {
        data: [{
            timestamp: RAW_TIMESTAMP,
            value: {
                Active: 40,
                'Sync Wait': 30,
                Preemption: 20,
                Unknown: 10,
                activeSeconds: 0.2,
                syncWaitSeconds: 0.15,
                preemptionSeconds: 0.1,
                unknownSeconds: 0.05,
                bucketWidthNs: 500_000_000,
            },
        }],
    };
});

const mapLanes = async (currentSession: Session): Promise<{ hostStart: number; stateTimestamp: number }> => {
    const hostUnit = new ThreadUnit(hostThreadMetadata);
    const stateUnit = new ThreadingStateUnit(stateMetadata);
    const hostData = await (hostUnit.chart as ChartDesc<'stackStatus'>).mapFunc(
        currentSession, hostThreadMetadata, hostUnit,
    );
    const stateData = await (stateUnit.chart as ChartDesc<'stackedBar'>).mapFunc(
        currentSession, stateMetadata, stateUnit,
    );
    return {
        hostStart: hostData[0][0].startTime,
        stateTimestamp: stateData[0].timestamp,
    };
};

describe('Thread State lane offset integration', () => {
    const originalRequest = window.request;

    afterEach(() => {
        window.request = originalRequest;
    });

    it('deduplicates concurrent requests and reuses the series across viewport changes', async () => {
        const currentSession = createSession();
        const requestMock = createRequestMock();
        window.request = requestMock as typeof window.request;
        const unit = new ThreadingStateUnit(stateMetadata);
        const chart = unit.chart as ChartDesc<'stackedBar'>;
        await Promise.all([chart.mapFunc(currentSession, stateMetadata, unit), chart.mapFunc(currentSession, stateMetadata, unit)]);
        currentSession.domain.zoom = { zoomCount: -1 };
        await chart.mapFunc(currentSession, stateMetadata, unit);
        expect(requestMock.mock.calls.filter(([, request]) => request.command === 'unit/counter')).toHaveLength(1);
        currentSession.unitsConfig.offsetConfig.timestampOffset[`${CARD_ID}__host`] = HOST_OFFSET;
        const shifted = await chart.mapFunc(currentSession, stateMetadata, unit);
        expect(shifted[0].timestamp).toBe(RAW_TIMESTAMP - HOST_OFFSET);
        expect(requestMock).toHaveBeenCalledTimes(2);
        await chart.mapFunc(currentSession, { ...stateMetadata, dbPath: 'another.db' }, unit);
        expect(requestMock).toHaveBeenCalledTimes(3);
    });

    it('uses the same positive width for the chart and tooltip data when metadata width is zero', async () => {
        const currentSession = createSession();
        window.request = createRequestMock() as typeof window.request;
        const metadata = { ...stateMetadata, bucketWidthNs: 0 };
        const unit = new ThreadingStateUnit(metadata);
        const chart = unit.chart as ChartDesc<'stackedBar'>;
        const data = await chart.mapFunc(currentSession, metadata, unit);
        const config = typeof chart.config === 'function' ? chart.config(currentSession, metadata) : chart.config;
        expect(config?.barWidth).toBe(500_000_000);
        expect(data[0]).toHaveProperty('bucketWidthNs', 500_000_000);
    });

    it('keeps Thread State aligned with its Host Thread and ignores Device Offset', async () => {
        const currentSession = createSession();
        const requestMock = createRequestMock();
        window.request = requestMock as typeof window.request;
        render(<CardOffsetConfig session={currentSession} metadata={cardMetadata} isHovered />);
        fireEvent.click(screen.getByTestId('offset-btn'));

        const hostInput = screen.getByLabelText('Host Offset');
        fireEvent.change(hostInput, { target: { value: String(HOST_OFFSET) } });
        fireEvent.blur(hostInput);

        const hostAligned = await mapLanes(currentSession);
        expect(hostAligned).toEqual({
            hostStart: RAW_TIMESTAMP - HOST_OFFSET,
            stateTimestamp: RAW_TIMESTAMP - HOST_OFFSET,
        });
        requestMock.mock.calls.slice(0, 2).forEach(([, request]: [DataSource, TimelineRequest]) => {
            expect(request.params.startTime).toBe(HOST_OFFSET);
            expect(request.params.endTime).toBe(SESSION_END + HOST_OFFSET);
        });

        const deviceInput = screen.getByLabelText('Device Offset');
        fireEvent.change(deviceInput, { target: { value: String(DEVICE_OFFSET) } });
        fireEvent.blur(deviceInput);

        expect(await mapLanes(currentSession)).toEqual(hostAligned);
        requestMock.mock.calls.slice(2, 4).forEach(([, request]: [DataSource, TimelineRequest]) => {
            expect(request.params.startTime).toBe(HOST_OFFSET);
            expect(request.params.endTime).toBe(SESSION_END + HOST_OFFSET);
        });
    });
});
