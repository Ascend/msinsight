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

import type { ChartHandle, Scale, StackStatusConfig, StackStatusData } from '../../../entity/chart';
import type { LabelMetaData, ThreadMetaData } from '../../../entity/data';
import type { ChartDesc, InsightUnit, SingleDataDesc } from '../../../entity/insight';
import type { ReactElement } from 'react';
import type { Theme } from '@emotion/react';
import type { ForegroundTarget, SearchData, Session } from '../../../entity/session';
import { renderRadiusBorder } from '../../../components/details/utils';
import {
    drawForegroundTargetLayer,
    drawSearchResultLayers,
    handleLinkLinesMap,
    isForegroundTargetSlice,
    isSearchMatched,
    LabelUnit,
    ThreadUnit,
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

describe('AscendUnit link line identity', () => {
    const createFlow = (): Parameters<typeof handleLinkLinesMap>[1] => ({
        cat: 'HostToDevice',
        id: 'flow',
        title: 'flow',
        from: { depth: 0, duration: 1, id: 'from', name: 'from', pid: '10', tid: '20', timestamp: 100, rankId: '', metaType: 'PROCESS' },
        to: { depth: 1, duration: 1, id: 'to', name: 'to', pid: '30', tid: '40', timestamp: 200, rankId: '', metaType: 'Ascend Hardware' },
    });

    it('keeps equal flow endpoints from different databases separate', () => {
        const session = { mapOfLinkLines: new Map() } as unknown as import('../../../entity/session').Session;

        handleLinkLinesMap(session, createFlow(), { rankId: 'rank0', dbPath: 'thread-1.db' });
        handleLinkLinesMap(session, createFlow(), { rankId: 'rank0', dbPath: 'thread-2.db' });

        expect(session.mapOfLinkLines.size).toBe(4);
        expect(Array.from(session.mapOfLinkLines.values()).every(value => value.current.dbPath !== undefined)).toBe(true);
    });

    it('maps backend endpoint ranks to the clicked frontend card identity', () => {
        const session = { mapOfLinkLines: new Map() } as unknown as import('../../../entity/session').Session;
        const flow = createFlow();
        flow.from.rankId = 'device-0';
        flow.to.rankId = 'device-0';

        handleLinkLinesMap(session, flow, { rankId: 'merged-card', dbPath: 'thread-1.db' });

        expect(flow.from.rankId).toBe('merged-card');
        expect(flow.to.rankId).toBe('merged-card');
    });
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

const createLabelMetadata = (overrides: Partial<LabelMetaData> = {}): LabelMetaData => ({
    dataSource: { remote: 'local' } as unknown as DataSource,
    cardId: 'rank0',
    dbPath: 'rank0.db',
    metaType: 'DPU',
    processId: 'DPU',
    processName: 'DPU',
    label: '',
    ...overrides,
});

const createSummarySession = (tryFetchFromCache: jest.Mock): Session => ({
    endTimeAll: 100,
    domain: { timePerPx: 1 },
    units: [],
    unitsConfig: { offsetConfig: { timestampOffset: {} } },
    simpleCache: { tryFetchFromCache },
} as unknown as Session);

describe('ThreadUnit trace layout', () => {
    const dataSource: DataSource = { remote: 'local', port: 9000, projectName: 'test', dataPath: [], projectPath: [], children: [] };
    const createSession = (autoAdjustUnitHeight = false): Session => ({
        units: [],
        domain: { timePerPx: 1 },
        domainRange: { domainStart: 0, domainEnd: 1000 },
        unitsConfig: { offsetConfig: { timestampOffset: {} }, filterConfig: { pythonFunction: {} } },
        autoAdjustUnitHeight,
    } as unknown as Session);

    it.each([
        ['PYTORCH_API_PYTHON_STACK', 'python_stack:text:123', false, 8],
        ['PYTORCH_API_PYTHON_STACK', 'python_stack:100', true, 3],
        ['PYTORCH_API', 'pytorch', false, 8],
        ['Ascend Hardware', '2', true, 3],
    ] as Array<[string, string, boolean, number]>)('preserves backend depths and height for %s (%s)', async (metaType, threadId, autoAdjust, expectedDepth) => {
        const metadata = { ...MERGED_METADATA, metaType, threadId, threadIdList: undefined } as ThreadMetaData;
        metadata.dataSource = dataSource;
        const unit = new ThreadUnit(metadata);
        const chart = unit.chart as ChartDesc<'stackStatus'>;
        const parent = createStackStatusData({ id: 'parent', startTime: 100, duration: 80, depth: 0 });
        const child = createStackStatusData({ id: 'child', startTime: 100, duration: 20, depth: 2 });
        const request = jest.fn().mockResolvedValue({ data: [[parent], [], [child]], maxDepth: 8, currentMaxDepth: 3 });
        window.request = request;

        const result = await chart.mapFunc(createSession(autoAdjust), metadata, unit);

        expect(result).toEqual([
            [expect.objectContaining({ id: 'parent', depth: 0 })], [], [expect.objectContaining({ id: 'child', depth: 2 })],
        ]);
        expect((chart.config as StackStatusConfig).maxDepth).toBe(expectedDepth);
        expect(request).toHaveBeenCalledWith(metadata.dataSource, {
            command: 'unit/threadTraces', params: expect.objectContaining({ threadId }),
        }, { silent: true });
        expect(unit.isTraceLoading).toBe(false);
    });

    it('keeps the thread list when the backend handles a merged lane', async () => {
        const metadata = { ...MERGED_METADATA, dataSource };
        const unit = new ThreadUnit(metadata);
        const request = jest.fn().mockResolvedValue({ data: [], maxDepth: 0, currentMaxDepth: 0 });
        window.request = request;

        await (unit.chart as ChartDesc<'stackStatus'>).mapFunc(createSession(), metadata, unit);

        expect(request).toHaveBeenCalledWith(metadata.dataSource, {
            command: 'unit/threadTraces', params: expect.objectContaining({ threadIdList: ['2', '8'] }),
        }, { silent: true });
    });

    it.each([
        ['python_stack:text:1704908', '1704908'],
        ['python_stack:100', 'pytorch'],
    ])('preserves %s through rendering, selection highlighting and the detail request', async (threadId, rawThreadId) => {
        const metadata = {
            ...MERGED_METADATA,
            metaType: 'PYTORCH_API_PYTHON_STACK',
            threadId,
            threadIdList: undefined,
            dataSource,
        };
        const unit = new ThreadUnit(metadata);
        const chart = unit.chart as ChartDesc<'stackStatus'>;
        const testSession = createSession();
        const detailResult = { title: 'python function', duration: 20, args: '{"Python id": 1}', rawStartTime: '100' };
        const request = jest.fn().mockResolvedValueOnce({
            data: [[createStackStatusData({ startTime: 100, threadId: rawThreadId })]], maxDepth: 1, currentMaxDepth: 1,
        }).mockResolvedValueOnce({ data: detailResult });
        window.request = request;

        const [[slice]] = await chart.mapFunc(testSession, metadata, unit);
        expect(slice.threadId).toBe(threadId);
        testSession.selectedData = { ...slice, threadId: slice.threadId ?? '', processId: metadata.processId ?? '', metaType: metadata.metaType };
        testSession.selectedDataUnit = unit;
        const ctx = {} as CanvasRenderingContext2D;
        await chart.decorator?.(testSession, metadata).action?.(
            { context: ctx } as ChartHandle<'stackStatus'>, value => value, value => value,
            { textColorPrimary: 'black' } as Theme,
        );
        expect(renderRadiusBorder).toHaveBeenCalledWith(expect.objectContaining({ ctx, depth: slice.depth }));

        const Detail = unit.bottomPanelRender?.(testSession, metadata)[0].Detail;
        const panel = Detail?.({ session: testSession, height: 300 }) as ReactElement<{
            detail: SingleDataDesc<Record<string, unknown>, ThreadMetaData>;
        }>;
        const result = await panel.props.detail.fetchData?.(testSession, metadata);

        expect(request).toHaveBeenLastCalledWith(metadata.dataSource, {
            command: 'unit/threadDetail',
            params: expect.objectContaining({
                tid: threadId, metaType: metadata.metaType, id: slice.id, startTime: 100, depth: slice.depth,
            }),
        });
        expect(result).toMatchObject(detailResult);
        expect(testSession.selectedData.rawStartTime).toBe('100');
    });

    it('lays out overlapping slices from explicit merged sources', async () => {
        const metadata: ThreadMetaData = {
            ...MERGED_METADATA,
            threadSourceList: ['2', '8'].map(threadId => ({
                cardId: '0', dbPath: 'trace.db', processId: '3513236896', threadId, threadName: `Stream ${threadId}`,
            })),
        };
        const unit = new ThreadUnit(metadata);
        const request = jest.fn().mockImplementation(async (_source, { params }) => ({
            data: [[createStackStatusData({ id: params.threadId, startTime: 100, depth: 0, threadId: params.threadId })]],
        }));
        window.request = request;

        const result = await (unit.chart as ChartDesc<'stackStatus'>).mapFunc(createSession(), metadata, unit);

        expect(result).toEqual([
            [expect.objectContaining({ id: '2', threadId: '2', depth: 0 })],
            [expect.objectContaining({ id: '8', threadId: '8', depth: 1 })],
        ]);
        expect(request).toHaveBeenCalledTimes(2);
    });
});

function createDeferred<T>(): { promise: Promise<T>; resolve: (value: T) => void } {
    let resolvePromise = (_value: T): void => {};
    const promise = new Promise<T>(resolve => {
        resolvePromise = resolve;
    });
    return { promise, resolve: resolvePromise };
}

describe('AscendUnit summary loading', () => {
    it('clears the initial skeleton when the summary request finishes', async () => {
        const metadata = createLabelMetadata();
        const unit = new LabelUnit(metadata);
        unit.children = [{
            name: 'Process',
            children: [{ name: 'Thread' } as unknown as InsightUnit],
        } as unknown as InsightUnit];
        const initialResult = createDeferred<undefined>();
        const tryFetchFromCache = jest.fn().mockReturnValueOnce(initialResult.promise);
        const session = createSummarySession(tryFetchFromCache);
        const summaryChart = unit.chart as ChartDesc<'status'>;

        const initialRequest = summaryChart.mapFunc(session, metadata, unit);
        expect(unit.isSummaryLoading).toBe(true);
        initialResult.resolve(undefined);
        await expect(initialRequest).resolves.toEqual([]);
        expect(unit.isSummaryLoading).toBe(false);
        expect(tryFetchFromCache).toHaveBeenCalledTimes(1);
    });

    it('skips summary requests for nested counter-only labels', async () => {
        const metadata = createLabelMetadata({ metaType: 'NPU_METRICS', processId: 'NPU Metrics', processName: 'NPU Metrics' });
        const unit = new LabelUnit(metadata);
        const metricGroup = new LabelUnit(createLabelMetadata({ processId: 'HBM', processName: 'HBM' }));
        metricGroup.children = [{ name: 'Counter' } as unknown as InsightUnit];
        unit.children = [metricGroup];
        const tryFetchFromCache = jest.fn();
        const session = createSummarySession(tryFetchFromCache);
        const summaryChart = unit.chart as ChartDesc<'status'>;

        const request = summaryChart.mapFunc(session, metadata, unit);
        expect(unit.isSummaryLoading).toBe(false);
        await expect(request).resolves.toEqual([]);
        expect(tryFetchFromCache).not.toHaveBeenCalled();
        expect(unit.isSummaryLoading).toBe(false);
    });
});
