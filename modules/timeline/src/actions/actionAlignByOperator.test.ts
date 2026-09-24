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

import { message } from 'antd';
import i18n from '@insight/lib/i18n';
import type { InsightUnit } from '../entity/insight';
import type { OffsetSide } from '../insight/units/offset';
import { Session, type SelectedDataType } from '../entity/session';
import {
    actionAlignByOperator,
    actionAlignByOperatorLeft,
    actionAlignByOperatorRight,
    applyAlignmentResult,
} from './actionAlignByOperator';
import { queryTimelineOffset } from '../api/request';
import type { QueryTimelineOffsetResult } from '../api/interface';
import {
    actionAlignToBenchmarkLeft,
    actionAlignToBenchmarkRight,
    actionClearBenchmarkSlice,
    actionSetBenchmarkSlice,
} from './actionSetBenchmarkSlice';

jest.mock('../api/request', () => ({
    queryTimelineOffset: jest.fn(),
}));

jest.mock('antd', () => ({
    message: {
        loading: (): (() => void) => () => {},
        warning: jest.fn(),
    },
}));

const queryTimelineOffsetMock = queryTimelineOffset as jest.MockedFunction<typeof queryTimelineOffset>;
const TEXT_SIDES: Array<{ side: OffsetSide; otherSide: OffsetSide; baseOffset: number; targetOffset: number }> = [
    { side: 'host', otherSide: 'device', baseOffset: 10, targetOffset: 20 },
    { side: 'device', otherSide: 'host', baseOffset: 80, targetOffset: 90 },
];

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

function createActionSession(metaType: string, offsetSide?: OffsetSide): Session {
    const base = {
        metadata: {
            cardId: 'base',
            processId: 'base-pid',
            dbPath: 'base.db',
            metaType,
            offsetSide,
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
        offsetSide,
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

function createBenchmarkSession(metaType: string, selectedDataOverrides: Partial<SelectedDataType> = {}, offsetSide?: OffsetSide): Session {
    const currentSession = createActionSession(metaType, offsetSide);
    actionSetBenchmarkSlice.perform(currentSession);
    const target = {
        metadata: {
            cardId: 'target',
            processId: 'target-pid',
            dbPath: 'target.db',
            metaType,
            offsetSide,
        },
    } as unknown as InsightUnit;
    currentSession.selectedUnits = [target];
    currentSession.selectedData = {
        cardId: 'target',
        processId: 'target-pid',
        threadId: 'target-tid',
        name: 'operator',
        metaType,
        offsetSide,
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

    it.each([
        { language: 'zhCN', automaticLabel: '时间对齐', benchmarkLabel: '与基准算子时间对齐' },
        { language: 'enUS', automaticLabel: 'Time Alignment', benchmarkLabel: 'Align to Base Slice' },
    ])('updates the $language parent label when the benchmark is set and cleared', ({ language, automaticLabel, benchmarkLabel }) => {
        const currentSession = createActionSession('CANN_API');
        const translate = i18n.getFixedT(language);

        expect(actionAlignByOperator.label(currentSession, translate)).toBe(automaticLabel);

        actionSetBenchmarkSlice.perform(currentSession);

        expect(actionAlignByOperator.label(currentSession, translate)).toBe(benchmarkLabel);

        actionClearBenchmarkSlice.perform(currentSession);

        expect(actionAlignByOperator.label(currentSession, translate)).toBe(automaticLabel);
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
    { direction: 'LEFT', action: actionAlignByOperatorLeft },
    { direction: 'RIGHT', action: actionAlignByOperatorRight },
])('automatic TEXT $direction alignment', ({ direction, action }) => {
    it.each(TEXT_SIDES)('uses the explicit $side offset without changing the TEXT request type', async ({ side, baseOffset }) => {
        const currentSession = createActionSession('TEXT', side);
        const offsetsBefore = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };
        queryTimelineOffsetMock.mockResolvedValueOnce({ result: [{ rankId: 'target', offset: 100 }], baseOffset: 30 });

        action.perform(currentSession);

        expect(queryTimelineOffsetMock).toHaveBeenCalledWith(expect.objectContaining({
            metaType: 'TEXT', alignType: direction, rankId: 'base', startTime: '100',
        }));
        await queryTimelineOffsetMock.mock.results[0].value;
        await Promise.resolve();
        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
            ...offsetsBefore,
            [`target__${side}`]: 100 + baseOffset - 30,
        });
        expect(currentSession.benchMarkData).toBeUndefined();
        expect(message.warning).not.toHaveBeenCalled();
    });

    it.each(TEXT_SIDES)('keeps the requested $side when the selection changes before the response', async ({ side, otherSide, baseOffset }) => {
        const currentSession = createActionSession('TEXT', side);
        const offsetsBefore = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };
        let resolveResponse: (result: QueryTimelineOffsetResult) => void = () => {};
        const response = new Promise<QueryTimelineOffsetResult>(resolve => { resolveResponse = resolve; });
        queryTimelineOffsetMock.mockReturnValueOnce(response);

        action.perform(currentSession);

        const selected = currentSession.selectedData;
        if (selected === undefined) {
            throw new Error('Expected the request operator to remain selected');
        }
        const nextSelection: SelectedDataType = { ...selected, id: 'another-operator', offsetSide: otherSide };
        currentSession.selectedData = nextSelection;
        resolveResponse({ result: [{ rankId: 'target', offset: 100 }], baseOffset: 30 });
        await response;
        await Promise.resolve();

        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
            ...offsetsBefore,
            [`target__${side}`]: 100 + baseOffset - 30,
        });
        expect(currentSession.selectedData).toEqual(nextSelection);
        expect(currentSession.benchMarkData).toBeUndefined();
        expect(queryTimelineOffsetMock).toHaveBeenCalledTimes(1);
    });
});

describe.each([
    { key: 'L', shortcut: actionAlignToBenchmarkLeft },
    { key: 'R', shortcut: actionAlignToBenchmarkRight },
])('$key without a benchmark', ({ shortcut }) => {
    beforeEach(() => {
        jest.clearAllMocks();
    });

    it.each([
        { selection: 'a selected operator', hasSelection: true },
        { selection: 'no selected operator', hasSelection: false },
    ])('stays silent and preserves state across repeated calls with $selection', ({ hasSelection }) => {
        const currentSession = createActionSession('CANN_API');
        if (!hasSelection) {
            currentSession.selectedData = undefined;
        }
        const offsetsBefore = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };
        const selectedBefore = currentSession.selectedData === undefined ? undefined : { ...currentSession.selectedData };
        const alignedBefore = [...currentSession.alignSliceData];
        const alignRenderBefore = currentSession.alignRender;

        for (let attempt = 0; attempt < 5; attempt++) {
            shortcut.perform(currentSession);
        }

        expect(message.warning).not.toHaveBeenCalled();
        expect(queryTimelineOffsetMock).not.toHaveBeenCalled();
        expect(currentSession.benchMarkData).toBeUndefined();
        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual(offsetsBefore);
        expect(currentSession.selectedData).toEqual(selectedBefore);
        expect(currentSession.alignSliceData).toEqual(alignedBefore);
        expect(currentSession.alignRender).toBe(alignRenderBefore);
    });
});

describe.each([
    { name: 'left menu', action: actionAlignByOperatorLeft },
    { name: 'right menu', action: actionAlignByOperatorRight },
    { name: 'L shortcut', action: actionAlignToBenchmarkLeft },
    { name: 'R shortcut', action: actionAlignToBenchmarkRight },
])('$name alignment highlight', ({ action }) => {
    it.each(['clear', 'replace'])('clears the target highlight when the selection changes (%s)', (selection) => {
        const currentSession = createBenchmarkSession('CANN_API');
        const benchmarkBefore = { ...currentSession.benchMarkData };

        action.perform(currentSession);

        expect(currentSession.alignSliceData).toHaveLength(1);
        const offsetsAfterAlignment = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };
        const renderAfterAlignment = currentSession.alignRender;
        const alignedSelection = currentSession.selectedData;
        if (alignedSelection === undefined) {
            throw new Error('Expected the aligned operator to remain selected');
        }
        const nextSelection: SelectedDataType | undefined = selection === 'clear'
            ? undefined
            : {
                ...alignedSelection,
                id: 'another-operator',
                startTime: 500,
            };

        currentSession.selectedData = nextSelection;

        expect(currentSession.alignSliceData).toEqual([]);
        expect(currentSession.selectedData).toEqual(nextSelection);
        expect(currentSession.benchMarkData).toEqual(benchmarkBefore);
        expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual(offsetsAfterAlignment);
        expect(currentSession.alignRender).toBe(renderAfterAlignment);
        expect(queryTimelineOffsetMock).not.toHaveBeenCalled();
    });
});

describe.each([
    { key: 'L', menu: actionAlignByOperatorLeft, shortcut: actionAlignToBenchmarkLeft, offsetDiff: 200, startTime: 100 },
    { key: 'R', menu: actionAlignByOperatorRight, shortcut: actionAlignToBenchmarkRight, offsetDiff: 230, startTime: 70 },
])('benchmark alignment menu matches $key', ({ key, menu, shortcut, offsetDiff, startTime }) => {
    it.each([
        { metaType: 'CANN_API', side: 'host', initialOffset: 20, offsetSide: undefined },
        { metaType: 'HCCL', side: 'device', initialOffset: 90, offsetSide: undefined },
        ...TEXT_SIDES.map(({ side, targetOffset }) => ({ metaType: 'TEXT', side, initialOffset: targetOffset, offsetSide: side })),
    ])('moves only the selected $metaType $side category and preserves the benchmark', ({ metaType, side, initialOffset, offsetSide }) => {
        const menuSession = createBenchmarkSession(metaType, {}, offsetSide);
        const shortcutSession = createBenchmarkSession(metaType, {}, offsetSide);
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

    it.each(TEXT_SIDES)('allows same-card TEXT alignment from $otherSide to $side', ({ side, otherSide, baseOffset }) => {
        for (const action of [menu, shortcut]) {
            const currentSession = createBenchmarkSession('TEXT', { cardId: 'base', offsetSide: side }, otherSide);
            const offsetsBefore = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };
            const benchmarkBefore = { ...currentSession.benchMarkData };

            action.perform(currentSession);

            expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual({
                ...offsetsBefore,
                [`base__${side}`]: baseOffset + offsetDiff,
            });
            expect(currentSession.selectedData?.startTime).toBe(startTime);
            expect(currentSession.benchMarkData).toEqual(benchmarkBefore);
        }
        expect(message.warning).not.toHaveBeenCalled();
        expect(queryTimelineOffsetMock).not.toHaveBeenCalled();
    });

    it.each(TEXT_SIDES)('rejects same-card TEXT alignment within the $side category', ({ side }) => {
        for (const action of [menu, shortcut]) {
            const currentSession = createBenchmarkSession('TEXT', { cardId: 'base' }, side);
            const offsetsBefore = { ...currentSession.unitsConfig.offsetConfig.timestampOffset };
            const selectedBefore = currentSession.selectedData === undefined ? undefined : { ...currentSession.selectedData };
            const benchmarkBefore = { ...currentSession.benchMarkData };

            action.perform(currentSession);

            expect(currentSession.unitsConfig.offsetConfig.timestampOffset).toEqual(offsetsBefore);
            expect(currentSession.selectedData).toEqual(selectedBefore);
            expect(currentSession.benchMarkData).toEqual(benchmarkBefore);
            expect(currentSession.alignSliceData).toEqual([]);
        }
        expect(message.warning).toHaveBeenCalledTimes(2);
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

describe('TEXT benchmark source category', () => {
    it.each(TEXT_SIDES)('retains the source unit $side when the selected slice has no explicit side', ({ side }) => {
        const currentSession = createActionSession('TEXT', side);
        const selected = currentSession.selectedData;
        if (selected === undefined) {
            throw new Error('Expected a selected TEXT operator');
        }
        selected.offsetSide = undefined;
        currentSession.selectedDataUnit = currentSession.selectedUnits[0];

        actionSetBenchmarkSlice.perform(currentSession);

        expect(currentSession.benchMarkData).toEqual(expect.objectContaining({ metaType: 'TEXT', offsetSide: side }));
    });
});
