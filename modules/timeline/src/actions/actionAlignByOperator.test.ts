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

import type { InsightUnit } from '../entity/insight';
import { Session, type SelectedDataType } from '../entity/session';
import {
    actionAlignByOperator,
    actionAlignByOperatorLeft,
    actionAlignByOperatorRight,
    applyAlignmentResult,
} from './actionAlignByOperator';
import { queryTimelineOffset } from '../api/request';
import {
    actionAlignToBenchmarkLeft,
    actionAlignToBenchmarkRight,
    actionSetBenchmarkSlice,
} from './actionSetBenchmarkSlice';

jest.mock('../api/request', () => ({
    queryTimelineOffset: jest.fn(),
}));

jest.mock('antd', () => ({
    message: {
        loading: (): (() => void) => () => {},
        warning: (): void => {},
    },
}));

const queryTimelineOffsetMock = queryTimelineOffset as jest.MockedFunction<typeof queryTimelineOffset>;

function createCard(cardId: string): InsightUnit {
    return { metadata: { cardId } } as unknown as InsightUnit;
}

function createSession(): Session {
    const currentSession = new Session({ units: [createCard('base'), createCard('target')] });
    currentSession.replaceTimestampOffsets({
        base__host: 10,
        base__device: 80,
        target__host: 20,
        target__device: 90,
    });
    return currentSession;
}

function createActionSession(metaType: string): Session {
    const base = {
        metadata: {
            cardId: 'base',
            processId: 'base-pid',
            dbPath: 'base.db',
            metaType,
        },
    } as unknown as InsightUnit;
    const currentSession = new Session({ units: [base, createCard('target')] });
    currentSession.selectedUnits = [base];
    currentSession.selectedData = {
        cardId: 'base',
        processId: 'base-pid',
        threadId: 'base-tid',
        name: 'operator',
        metaType,
        rawStartTime: '100',
        startTime: 100,
        duration: 20,
    };
    currentSession.replaceTimestampOffsets({
        base__host: 10,
        base__device: 80,
        target__host: 20,
        target__device: 90,
    });
    return currentSession;
}

function createBenchmarkSession(metaType: string, selectedDataOverrides: Partial<SelectedDataType> = {}): Session {
    const currentSession = createActionSession(metaType);
    actionSetBenchmarkSlice.perform(currentSession);
    const target = {
        metadata: {
            cardId: 'target',
            processId: 'target-pid',
            dbPath: 'target.db',
            metaType,
        },
    } as unknown as InsightUnit;
    currentSession.selectedUnits = [target];
    currentSession.selectedData = {
        cardId: 'target',
        processId: 'target-pid',
        threadId: 'target-tid',
        name: 'operator',
        metaType,
        rawStartTime: '300',
        startTime: 300,
        duration: 50,
        ...selectedDataOverrides,
    };
    return currentSession;
}

describe('automatic alignment result application', () => {
    it('updates only the selected Host category', () => {
        const currentSession = createSession();

        const applied = applyAlignmentResult(currentSession, [{ rankId: 'target', offset: 100 }], {
            selectedSide: 'host',
            offsetDelta: 10,
        });

        expect(applied).toBe(true);
        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
            base__host: 10,
            base__device: 80,
            target__host: 110,
            target__device: 90,
        });
    });

    it('updates only the selected Device category', () => {
        const currentSession = createSession();

        const applied = applyAlignmentResult(currentSession, [{ rankId: 'target', offset: 120 }], {
            selectedSide: 'device',
            offsetDelta: -20,
        });

        expect(applied).toBe(true);
        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
            base__host: 10,
            base__device: 80,
            target__host: 20,
            target__device: 100,
        });
    });

    it('ignores invalid results and leaves all offsets unchanged when none are valid', () => {
        const currentSession = createSession();
        const before = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };

        const applied = applyAlignmentResult(currentSession, [
            { rankId: '', offset: 100 },
            { rankId: 'target', offset: Number.NaN },
        ], {
            selectedSide: 'host',
            offsetDelta: 10,
        });

        expect(applied).toBe(false);
        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual(before);
    });
});

describe('automatic alignment menu', () => {
    beforeEach(() => {
        queryTimelineOffsetMock.mockResolvedValue({
            result: [{ rankId: 'target', offset: 100 }],
            baseOffset: 0,
        });
    });

    it('exposes Align Left and Align Right as terminal submenu items', () => {
        expect(actionAlignByOperator.subMenus?.()).toEqual([
            actionAlignByOperatorLeft,
            actionAlignByOperatorRight,
        ]);
        expect(actionAlignByOperatorLeft).not.toHaveProperty('subMode');
        expect(actionAlignByOperatorLeft).not.toHaveProperty('subMenus');
        expect(actionAlignByOperatorRight).not.toHaveProperty('subMode');
        expect(actionAlignByOperatorRight).not.toHaveProperty('subMenus');
    });

    it('hides operator alignment for DPU tasks', () => {
        const currentSession = createActionSession('DPU');

        expect(actionAlignByOperator.visible?.(currentSession)).toBe(false);
        expect(actionAlignByOperatorLeft.visible?.(currentSession)).toBe(false);
        expect(actionAlignByOperatorRight.visible?.(currentSession)).toBe(false);
    });

    it('runs left alignment directly for the selected Host side', async () => {
        const currentSession = createActionSession('CANN_API');

        actionAlignByOperatorLeft.perform(currentSession);

        expect(queryTimelineOffsetMock).toHaveBeenCalledWith(expect.objectContaining({ alignType: 'LEFT' }));
        await queryTimelineOffsetMock.mock.results[0].value;
        await Promise.resolve();
        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
            base__host: 10,
            base__device: 80,
            target__host: 110,
            target__device: 90,
        });
    });

    it('runs right alignment directly for the selected Device side', async () => {
        const currentSession = createActionSession('HCCL');

        actionAlignByOperatorRight.perform(currentSession);

        expect(queryTimelineOffsetMock).toHaveBeenCalledWith(expect.objectContaining({ alignType: 'RIGHT' }));
        await queryTimelineOffsetMock.mock.results[0].value;
        await Promise.resolve();
        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
            base__host: 10,
            base__device: 80,
            target__host: 20,
            target__device: 180,
        });
    });
});

describe.each([
    { key: 'L', menu: actionAlignByOperatorLeft, shortcut: actionAlignToBenchmarkLeft, offsetDiff: 200, startTime: 100 },
    { key: 'R', menu: actionAlignByOperatorRight, shortcut: actionAlignToBenchmarkRight, offsetDiff: 230, startTime: 70 },
])('benchmark alignment menu matches $key', ({ key, menu, shortcut, offsetDiff, startTime }) => {
    it.each([
        { metaType: 'CANN_API', side: 'host', initialOffset: 20 },
        { metaType: 'HCCL', side: 'device', initialOffset: 90 },
    ])('moves only the selected $side category and preserves the benchmark', ({ metaType, side, initialOffset }) => {
        const menuSession = createBenchmarkSession(metaType);
        const shortcutSession = createBenchmarkSession(metaType);
        const offsetsBefore = { ...menuSession.unitsConfig.offsetConfig.timestampOffset };
        const benchmarkBefore = { ...menuSession.benchMarkData };

        expect(menu.visible?.(menuSession)).toBe(true);
        menu.perform(menuSession);
        expect(shortcut.keyTest(new KeyboardEvent('keydown', { key }))).toBe(true);
        shortcut.perform(shortcutSession);

        expect(menuSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
            ...offsetsBefore,
            [`target__${side}`]: initialOffset + offsetDiff,
        });
        expect(menuSession.benchMarkData).toEqual(benchmarkBefore);
        expect(menuSession.selectedData?.startTime).toBe(startTime);
        expect(menuSession.unitsConfig.offsetConfig.timestampOffset).toEqual(shortcutSession.unitsConfig.offsetConfig.timestampOffset);
        expect(menuSession.selectedData).toEqual(shortcutSession.selectedData);
        expect(menuSession.alignSliceData).toEqual(shortcutSession.alignSliceData);
        expect(menuSession.alignRender).toBe(shortcutSession.alignRender);
        expect(queryTimelineOffsetMock).not.toHaveBeenCalled();
    });

    it('allows alignment between Host and Device on the same card', () => {
        const currentSession = createBenchmarkSession('CANN_API', { cardId: 'base', metaType: 'HCCL' });
        const offsetsBefore = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };
        const benchmarkBefore = { ...currentSession.benchMarkData };

        menu.perform(currentSession);

        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
            ...offsetsBefore,
            base__device: 80 + offsetDiff,
        });
        expect(currentSession.benchMarkData).toEqual(benchmarkBefore);
        expect(currentSession.selectedData?.startTime).toBe(startTime);
        expect(queryTimelineOffsetMock).not.toHaveBeenCalled();
    });

    it.each(['CANN_API', 'HCCL'])('leaves the same card and %s category unchanged', (metaType) => {
        const currentSession = createBenchmarkSession(metaType, { cardId: 'base' });
        const offsetsBefore = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };
        const benchmarkBefore = { ...currentSession.benchMarkData };
        const selectedBefore = { ...currentSession.selectedData };

        menu.perform(currentSession);

        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual(offsetsBefore);
        expect(currentSession.benchMarkData).toEqual(benchmarkBefore);
        expect(currentSession.selectedData).toEqual(selectedBefore);
        expect(currentSession.alignSliceData).toEqual([]);
        expect(queryTimelineOffsetMock).not.toHaveBeenCalled();
    });

    it('aligns to the benchmark when the selected operator has no raw start time', () => {
        const currentSession = createBenchmarkSession('CANN_API', { rawStartTime: undefined });
        const offsetsBefore = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };
        const benchmarkBefore = { ...currentSession.benchMarkData };

        menu.perform(currentSession);

        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
            ...offsetsBefore,
            target__host: 20 + offsetDiff,
        });
        expect(currentSession.benchMarkData).toEqual(benchmarkBefore);
        expect(currentSession.selectedData?.startTime).toBe(startTime);
        expect(queryTimelineOffsetMock).not.toHaveBeenCalled();
    });
});
