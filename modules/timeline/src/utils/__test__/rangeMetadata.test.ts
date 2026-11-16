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

import type { InsightUnit } from '../../entity/insight';
import type { Session } from '../../entity/session';
import {
    getTimeAnalysisFlowRanges,
    getTimeAnalysisRangeMetadataList,
    isTimeAnalysisRangeRestricted,
} from '../rangeMetadata';

interface LaneOptions {
    cardId?: string;
    dbPath?: string;
    processId?: string;
    threadId?: string;
    threadIdList?: string[];
    metaType?: string;
    offsetSide?: 'host' | 'device';
    isDisplay?: boolean;
    isMultiDeviceHidden?: boolean;
    threadSourceList?: Array<{
        cardId: string;
        dbPath: string;
        processId?: string;
        threadId: string;
        metaType?: string;
    }>;
}

const createUnit = (options: LaneOptions): InsightUnit => ({
    metadata: {
        cardId: options.cardId ?? '0',
        dbPath: options.dbPath ?? 'card.db',
        processId: options.processId ?? '100',
        threadId: options.threadId ?? '',
        threadIdList: options.threadIdList,
        threadSourceList: options.threadSourceList,
        metaType: options.metaType ?? 'Ascend Hardware',
        offsetSide: options.offsetSide,
        dataSource: 'local',
    },
    isDisplay: options.isDisplay ?? true,
    isMultiDeviceHidden: options.isMultiDeviceHidden ?? false,
} as unknown as InsightUnit);

const createCard = (children: InsightUnit[]): InsightUnit => ({
    metadata: {
        cardId: '0',
        dbPath: 'card.db',
        dataSource: 'local',
    },
    children,
    isDisplay: true,
    isMultiDeviceHidden: false,
    isMerged: false,
} as unknown as InsightUnit);

const createSession = (units: InsightUnit[], overrides: Partial<Session> = {}): Session => ({
    units,
    lockUnit: [],
    lockRange: undefined,
    isTimeAnalysisMode: false,
    timeAnalysisRange: undefined,
    unitsConfig: {
        offsetConfig: {
            timestampOffset: {},
        },
    },
    ...overrides,
} as unknown as Session);

describe('range metadata', () => {
    it('groups flow requests by lane offset, including the raw viewport coordinates', () => {
        const session = createSession([
            createUnit({ threadId: 'host', metaType: 'TEXT', offsetSide: 'host' }),
            createUnit({ threadId: 'device', offsetSide: 'device' }),
            createUnit({ threadId: 'device2', offsetSide: 'device' }),
            createUnit({ cardId: 'other', threadId: 'other' }),
        ], {
            isTimeAnalysisMode: true,
            timeAnalysisRange: [10, 20],
            unitsConfig: { offsetConfig: { timestampOffset: { '0__host': 0, '0__device': 100 } } },
        } as unknown as Partial<Session>);
        const groups = getTimeAnalysisFlowRanges(getTimeAnalysisRangeMetadataList(session), '0', 'card.db', [10, 20], 0, 200);

        expect(groups).toEqual([
            {
                metadataList: [expect.objectContaining({ tid: 'host' })],
                lockStartTime: 10,
                lockEndTime: 20,
                startTime: 0,
                endTime: 200,
            },
            {
                metadataList: [expect.objectContaining({ tid: 'device' }), expect.objectContaining({ tid: 'device2' })],
                lockStartTime: 110,
                lockEndTime: 120,
                startTime: 100,
                endTime: 300,
            },
        ]);
        const deviceRange = groups[1];
        // The visible Device endpoint at 15 has raw timestamp 115, not 15.
        expect(deviceRange.lockStartTime <= 115 && deviceRange.lockEndTime >= 115).toBe(true);
        expect(deviceRange.lockStartTime <= 15 && deviceRange.lockEndTime >= 15).toBe(false);
    });

    it('uses one flow request for a shared offset and none for an absent card', () => {
        const session = createSession([createUnit({ threadIdList: ['1', '2', '3'] })], {
            isTimeAnalysisMode: true, timeAnalysisRange: [10, 20],
        });
        const metadata = getTimeAnalysisRangeMetadataList(session);
        expect(getTimeAnalysisFlowRanges(metadata, '0', 'card.db', [10, 20], 0, 200)).toHaveLength(1);
        expect(getTimeAnalysisFlowRanges(metadata, 'missing', 'card.db', [10, 20], 0, 200)).toEqual([]);
    });

    it('does not authorize a hidden source using another source with the same PID/TID', () => {
        const session = createSession([
            createUnit({ dbPath: 'a.db', threadId: '1' }),
            createUnit({ dbPath: 'b.db', threadId: '1', isDisplay: false }),
        ], { isTimeAnalysisMode: true, timeAnalysisRange: [10, 20] });
        const metadata = getTimeAnalysisRangeMetadataList(session);

        expect(getTimeAnalysisFlowRanges(metadata, '0', 'a.db', [10, 20], 0, 200)).toHaveLength(1);
        expect(getTimeAnalysisFlowRanges(metadata, '0', 'b.db', [10, 20], 0, 200)).toEqual([]);
    });

    it.each([false, true])('preserves explicit TEXT device offsets (merged=%s)', merged => {
        const session = createSession([createUnit({
            threadId: '1',
            metaType: 'TEXT',
            offsetSide: 'device',
            threadSourceList: merged
                ? [{ cardId: '0', dbPath: 'card.db', processId: '100', threadId: '2' }]
                : undefined,
        })], {
            isTimeAnalysisMode: true,
            timeAnalysisRange: [10.4, 20.4],
            unitsConfig: { offsetConfig: { timestampOffset: { '0__host': 1, '0__device': 100 } } },
        } as unknown as Partial<Session>);

        expect(getTimeAnalysisRangeMetadataList(session)).toEqual([
            expect.objectContaining({ tid: merged ? '2' : '1', lockStartTime: 110, lockEndTime: 121 }),
        ]);
    });

    it('does not traverse hidden data sources or mutate rendering levels', () => {
        const visible = createCard([createUnit({ threadId: '1' })]);
        const hidden = createCard([createUnit({ threadId: '2' })]);
        hidden.isDisplay = false;
        const session = createSession([visible, hidden], {
            isTimeAnalysisMode: true, timeAnalysisRange: [10, 20],
        });
        const keys = Reflect.ownKeys(visible);

        expect(getTimeAnalysisRangeMetadataList(session).map(item => item.tid)).toEqual(['1']);
        expect(Reflect.ownKeys(visible)).toEqual(keys);
    });

    it('collects lanes nested under card units and restricts them to the analysis time range', () => {
        const session = createSession([
            createCard([
                createUnit({ threadId: '1' }),
                createUnit({ threadId: '2' }),
            ]),
        ], { isTimeAnalysisMode: true, timeAnalysisRange: [100, 200] });

        const metadataList = getTimeAnalysisRangeMetadataList(session);

        expect(metadataList).toHaveLength(2);
        expect(metadataList[0]).toEqual(expect.objectContaining({
            tid: '1',
            pid: '100',
            rankId: '0',
            dbPath: 'card.db',
            lockStartTime: 100,
            lockEndTime: 200,
        }));
        expect(metadataList[1]).toEqual(expect.objectContaining({ tid: '2' }));
    });

    it('ignores units that have no track identity', () => {
        const session = createSession([createCard([])], {
            isTimeAnalysisMode: true,
            timeAnalysisRange: [100, 200],
        });

        expect(getTimeAnalysisRangeMetadataList(session)).toEqual([]);
    });

    it('applies the card timestamp offset to the analysis range', () => {
        const session = createSession([createUnit({ threadId: '1' })], {
            isTimeAnalysisMode: true,
            timeAnalysisRange: [100.4, 200.4],
            unitsConfig: { offsetConfig: { timestampOffset: { '0__device': 5 } } },
        } as unknown as Partial<Session>);

        const [metadata] = getTimeAnalysisRangeMetadataList(session);

        expect(metadata.lockStartTime).toBe(105);
        expect(metadata.lockEndTime).toBe(206);
    });

    it('keeps merged lanes searchable through their source threads', () => {
        const session = createSession([
            createUnit({ threadIdList: ['11', '12'], threadId: '' }),
            createUnit({
                threadId: '',
                threadSourceList: [
                    { cardId: '0', dbPath: 'first.db', processId: '100', threadId: '21', metaType: 'Ascend Hardware' },
                    { cardId: '1', dbPath: 'second.db', processId: '200', threadId: '22', metaType: 'Ascend Hardware' },
                ],
            }),
        ], { isTimeAnalysisMode: true, timeAnalysisRange: [10, 20] });

        const metadataList = getTimeAnalysisRangeMetadataList(session);

        expect(metadataList.map(item => [item.rankId, item.dbPath, item.pid, item.tid])).toEqual([
            ['0', 'card.db', '100', '11'],
            ['0', 'card.db', '100', '12'],
            ['0', 'first.db', '100', '21'],
            ['1', 'second.db', '200', '22'],
        ]);
    });

    it('does not return the same lane twice', () => {
        const unit = createUnit({ threadId: '1' });
        const session = createSession([unit, unit], {
            isTimeAnalysisMode: true,
            timeAnalysisRange: [10, 20],
        });

        expect(getTimeAnalysisRangeMetadataList(session)).toHaveLength(1);
    });

    it('does not handle locked selection when time analysis is disabled', () => {
        const session = createSession([createUnit({ threadId: '1' })], {
            lockUnit: [createUnit({ threadId: '9', dbPath: 'locked.db' })],
            lockRange: [50, 80],
        });

        expect(getTimeAnalysisRangeMetadataList(session)).toEqual([]);
    });

    it('switches to the analysis range only when time range analysis is enabled', () => {
        const session = createSession([createUnit({ threadId: '1' })], {
            lockUnit: [createUnit({ threadId: '9', dbPath: 'locked.db' })],
            lockRange: [50, 80],
            isTimeAnalysisMode: true,
            timeAnalysisRange: [10, 20],
        });

        expect(isTimeAnalysisRangeRestricted(session)).toBe(true);
        expect(getTimeAnalysisRangeMetadataList(session)).toEqual([
            expect.objectContaining({ tid: '1', lockStartTime: 10, lockEndTime: 20 }),
        ]);

        session.isTimeAnalysisMode = false;

        expect(isTimeAnalysisRangeRestricted(session)).toBe(false);
        expect(getTimeAnalysisRangeMetadataList(session)).toEqual([]);
    });
});
