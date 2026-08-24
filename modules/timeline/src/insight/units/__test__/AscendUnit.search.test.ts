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

import type { ChartHandle, Scale, StackStatusData } from '../../../entity/chart';
import type { ThreadMetaData } from '../../../entity/data';
import type { ForegroundTarget, SearchData } from '../../../entity/session';
import {
    drawForegroundTargetLayer,
    drawSearchResultLayers,
    isForegroundTargetSlice,
    isSearchMatched,
} from '../AscendUnit';

jest.mock('@insight/lib/resize', () => ({
    ResizeTable: (): null => null,
    fetchColumnFilterProps: (): Record<string, never> => ({}),
}), { virtual: true });
jest.mock('@insight/lib', () => ({
    DragDirection: {},
    useDraggableContainer: jest.fn(),
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
jest.mock('../unitFunc', () => ({
    createCounterParam: (): string => '',
    createStatusParam: (): string => '',
}));
jest.mock('../config/offsetConfig', () => ({ cardOffsetConfig: jest.fn() }));
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

const createStackStatusData = (overrides: Partial<StackStatusData> = {}): StackStatusData => ({
    id: 'slice',
    startTime: 0,
    originalStartTime: 100,
    duration: 20,
    name: 'operator',
    type: 'operator',
    color: 'deepBlue',
    depth: 0,
    cname: '',
    threadId: '2',
    ...overrides,
});

const createForegroundTarget = (overrides: Partial<ForegroundTarget> = {}): ForegroundTarget => ({
    rankId: '0',
    dbPath: 'trace.db',
    pid: '3513236896',
    tid: '2',
    id: 'target',
    name: 'TargetOperator',
    startTime: 100,
    duration: 20,
    depth: 0,
    metaType: 'Ascend Hardware',
    ...overrides,
});

const MERGED_METADATA = {
    cardId: '0',
    dbPath: 'trace.db',
    processId: '3513236896',
    metaType: 'Ascend Hardware',
    threadId: '',
    threadIdList: ['2', '8'],
    threadName: 'Stream Merged (2, 8)',
    groupNameValue: '',
    rankList: [],
} as unknown as ThreadMetaData;

describe('AscendUnit search and foreground drawing', () => {
    it.each([
        [{ content: 'target', isMatchCase: false, isMatchExact: false }, 'MyTargetOperator', true],
        [{ content: 'target', isMatchCase: true, isMatchExact: false }, 'MyTargetOperator', false],
        [{ content: 'targetoperator', isMatchCase: false, isMatchExact: true }, 'TargetOperator', true],
        [{ content: 'Target', isMatchCase: true, isMatchExact: true }, 'TargetOperator', false],
    ] as Array<[SearchData, string, boolean]>)('matches according to case and exact options', (searchData, name, expected) => {
        expect(isSearchMatched(searchData, { name })).toBe(expected);
    });

    it.each([undefined, null])('does not match or throw when the runtime item name is %p', (name) => {
        const searchData: SearchData = {
            content: 'TargetOperator',
            isMatchCase: false,
            isMatchExact: false,
        };
        const runtimeItem = { name } as unknown as Pick<StackStatusData, 'name'>;

        expect(() => isSearchMatched(searchData, runtimeItem)).not.toThrow();
        expect(isSearchMatched(searchData, runtimeItem)).toBe(false);
    });

    it.each([undefined, null])('does not draw search layers when the runtime content is %p', (content) => {
        const handle = {
            context: null,
            draw: jest.fn(),
            findAll: jest.fn(),
        } as unknown as ChartHandle<'stackStatus'>;
        const searchData = {
            content,
            isMatchCase: false,
            isMatchExact: false,
        } as unknown as SearchData;
        const scale: Scale = value => value;

        drawSearchResultLayers(searchData, handle, scale, scale);
        expect(handle.findAll).not.toHaveBeenCalled();
        expect(handle.draw).not.toHaveBeenCalled();
    });

    it('draws mask, matches, and a non-matching foreground target in that order', () => {
        const coveredTarget = createStackStatusData({ id: 'target', name: 'OtherOperator', threadId: '2' });
        const anotherMatch = createStackStatusData({ id: 'another-match', name: 'TargetOperator', threadId: '8' });
        const coveringNonMatch = createStackStatusData({ id: 'cover', name: 'EVENT_WAIT', threadId: '8' });
        const data = [[coveredTarget, anotherMatch, coveringNonMatch]];
        const draw = jest.fn();
        const handle = {
            context: null,
            draw,
            findAll: (predicate): StackStatusData[][] => data.map(row => row.filter(predicate)),
        } as ChartHandle<'stackStatus'>;
        const scale: Scale = value => value;
        const searchData: SearchData = {
            content: 'TargetOperator',
            isMatchCase: true,
            isMatchExact: true,
        };
        const target = createForegroundTarget({ name: coveredTarget.name });

        drawSearchResultLayers(searchData, handle, scale, scale);
        const result = drawForegroundTargetLayer(target, handle, MERGED_METADATA, scale, scale);

        expect(draw).toHaveBeenCalledTimes(3);
        const maskedLayer = draw.mock.calls[0][0] as StackStatusData[][];
        expect(maskedLayer[0]).toEqual([
            expect.objectContaining({ id: 'target', color: 'transparentMask' }),
            expect.objectContaining({ id: 'cover', color: 'transparentMask' }),
        ]);
        expect(draw.mock.calls[1][0]).toEqual([[anotherMatch]]);
        expect(draw.mock.calls[2][0]).toEqual([[coveredTarget]]);
        expect(result).toBe(coveredTarget);
    });

    it('draws the foreground target when there is no active search', () => {
        const coveredTarget = createStackStatusData({ id: 'target', name: 'OtherOperator', threadId: '2' });
        const coveringSlice = createStackStatusData({ id: 'cover', name: 'CoveringOperator', threadId: '8' });
        const data = [[coveredTarget, coveringSlice]];
        const draw = jest.fn();
        const handle = {
            context: null,
            draw,
            findAll: (predicate): StackStatusData[][] => data.map(row => row.filter(predicate)),
        } as ChartHandle<'stackStatus'>;
        const scale: Scale = value => value;

        drawSearchResultLayers(undefined, handle, scale, scale);
        const result = drawForegroundTargetLayer(
            createForegroundTarget({ name: coveredTarget.name }), handle, MERGED_METADATA, scale, scale,
        );

        expect(draw).toHaveBeenCalledTimes(1);
        expect(draw).toHaveBeenCalledWith([[coveredTarget]], scale, scale);
        expect(result).toBe(coveredTarget);
    });

    it('identifies a target without an id by its source thread and slice fields', () => {
        const target = createForegroundTarget({ id: undefined });
        const matchingSlice = createStackStatusData({
            id: undefined,
            name: target.name,
            threadId: target.tid,
            originalStartTime: target.startTime,
            duration: target.duration,
            depth: target.depth,
        });

        expect(isForegroundTargetSlice(matchingSlice, target)).toBe(true);
        expect(isForegroundTargetSlice({ ...matchingSlice, threadId: '8' }, target)).toBe(false);
        expect(isForegroundTargetSlice({ ...matchingSlice, originalStartTime: target.startTime + 1 }, target)).toBe(false);
    });

    it.each([
        ['rank', { rankId: '1' }],
        ['database', { dbPath: 'other.db' }],
        ['process', { pid: 'other-pid' }],
        ['thread', { tid: '99' }],
        ['metadata type', { metaType: 'Other Hardware' }],
    ] as Array<[string, Partial<ForegroundTarget>]>)('does not draw a target from a different %s', (_label, overrides) => {
        const handle = {
            context: null,
            draw: jest.fn(),
            findAll: jest.fn(),
        } as unknown as ChartHandle<'stackStatus'>;
        const scale: Scale = value => value;

        expect(drawForegroundTargetLayer(
            createForegroundTarget(overrides), handle, MERGED_METADATA, scale, scale,
        )).toBeUndefined();
        expect(handle.findAll).not.toHaveBeenCalled();
        expect(handle.draw).not.toHaveBeenCalled();
    });

    it.each([undefined, null])('does not query or draw when the foreground target is %p', (target) => {
        const handle = {
            context: null,
            draw: jest.fn(),
            findAll: jest.fn(),
        } as unknown as ChartHandle<'stackStatus'>;
        const scale: Scale = value => value;

        expect(drawForegroundTargetLayer(target, handle, MERGED_METADATA, scale, scale)).toBeUndefined();
        expect(handle.findAll).not.toHaveBeenCalled();
        expect(handle.draw).not.toHaveBeenCalled();
    });

    it('does not match an id-less target whose composite identity is incomplete', () => {
        const target = createForegroundTarget({ id: undefined, startTime: undefined as unknown as number });
        const item = createStackStatusData({ id: undefined, threadId: target.tid, originalStartTime: undefined });

        expect(isForegroundTargetSlice(item, target)).toBe(false);
    });
});
