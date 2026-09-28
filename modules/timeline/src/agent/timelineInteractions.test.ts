/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { runInAction } from 'mobx';
import { message } from 'antd';
import type { CommandDefinition, CommandHandler, JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { unit, type InsightUnit } from '../entity/insight';
import type { CardMetaData, ThreadMetaData } from '../entity/data';
import type { StackStatusData } from '../entity/chart';
import { Session } from '../entity/session';
import { isPinned, switchPinned } from '../components/ChartContainer/unitPin';
import { selectRenderedSlice } from '../utils/selectRenderedSlice';
import { registerVisibleSliceSource, visibleSliceScope, type VisibleSliceSource } from './visibleSlices';
import { observeTimeline, registerTimelineCommands, setTimelineAgentSession } from './timelineController';

const Thread = unit<ThreadMetaData>({ name: 'Thread' });
const Card = unit<CardMetaData>({ name: 'Card' });
const dataSource = { remote: '', port: 0, projectName: '', dataPath: [], projectPath: [], children: [] };
const createThread = (threadId: string, threadName = 'stream', cardId = '0'): InsightUnit => new Thread({
    dataSource, cardId, dbPath: '/private/data.db', threadId, threadName, processId: 'p1', metaType: 'TEXT', rankList: [], groupNameValue: '',
});
const createCard = (cardId: string, children: InsightUnit[]): InsightUnit => {
    const card = new Card({ dataSource, cardId, dbPath: '/private/data.db', cluster: '', cardName: cardId, cardPath: '' });
    card.children = children;
    card.phase = 'download';
    return card;
};
const slice: StackStatusData = {
    id: 'slice-1',
    cardId: '0',
    dbPath: '/private/data.db',
    name: 'MatMul',
    type: 'MatMul',
    startTime: 1000,
    originalStartTime: 1100,
    duration: 50,
    depth: 0,
    color: 'deepBlue',
    cname: '',
};

let session: Session;
let handlers: Map<string, CommandHandler>;
let stops: Array<() => void>;
let originalRequest: typeof window.request;
const invoke = (name: string, args: JsonObject = {}, signal = new AbortController().signal, deadline = Date.now() + 5000): unknown =>
    handlers.get(name)?.(args, { requestId: 'test', deadline, signal });
const select = (args: JsonObject): Promise<any> => invoke('Timeline.selectSlice', args) as Promise<any>;
const rect = (top = 100, height = 80): DOMRect => ({ top, bottom: top + height, left: 200, right: 1000, width: 800, height } as DOMRect);

const mountSource = (data = [slice], lane = session.units[0].children?.[0] as InsightUnit, top = 100): VisibleSliceSource => {
    const element = document.createElement('div');
    element.getBoundingClientRect = () => rect(top);
    document.body.appendChild(element);
    const source: VisibleSliceSource = {
        unit: lane,
        element,
        data: [data],
        ...session.domainRange,
        rowHeight: 20,
        ready: true,
        scope: visibleSliceScope(session, lane),
        select: jest.fn(item => runInAction(() => {
            session.selectedUnits = [lane];
            selectRenderedSlice(session, lane.metadata as ThreadMetaData, item);
        })),
    };
    const unregister = registerVisibleSliceSource(session, source);
    stops.push(() => { unregister(); element.remove(); });
    return source;
};

beforeEach(() => {
    session = new Session({ isNsMode: true });
    session.phase = 'download';
    session.domainRange = { domainStart: 0, domainEnd: 2000 };
    session.units = [createCard('0', [createThread('t1'), createThread('t2'), createThread('t3', 'other')])];
    setTimelineAgentSession(session);
    handlers = new Map();
    stops = [registerTimelineCommands({
        registerCommand(definition: CommandDefinition, handler: CommandHandler) {
            handlers.set(definition.name, handler);
            return () => handlers.delete(definition.name);
        },
    } as unknown as ModuleAgentCommandClient)];
    originalRequest = window.request;
    window.request = jest.fn();
});

afterEach(() => {
    stops.forEach(stop => stop());
    setTimelineAgentSession(undefined);
    session.cancelZoomingHistory();
    window.request = originalRequest;
    jest.restoreAllMocks();
});

describe('select visible slice', () => {
    test('selects one rendered operator and its lane without searching or changing the viewport', async () => {
        mountSource();
        const viewport = { ...session.domainRange };
        await expect(select({ name: 'MatMul' })).resolves.toMatchObject({ status: 'selected', totalCount: 1, requiresObserve: true });
        expect(session.selectedData).toMatchObject({ id: slice.id, name: slice.name, timestamp: 1100, showSelectedData: true });
        expect(session.selectedUnits).toEqual([session.units[0].children?.[0]]);
        expect(session.domainRange).toEqual(viewport);
        expect(window.request).not.toHaveBeenCalled();
        expect(invoke('Timeline.pinByUnitName')).toMatchObject({ addedCount: 2 });
    });

    test('lists duplicate names without changing selection, then selects the chosen stable reference', async () => {
        mountSource([slice, { ...slice, id: 'slice-2', startTime: 1500, duration: 100 }]);
        const before = { ...session.domainRange };
        const result = await select({ name: 'MatMul' });
        expect(result).toMatchObject({ status: 'needsChoice', needsChoice: true, totalCount: 2, timeUnit: 'ns' });
        expect(result.candidates).toEqual([
            expect.objectContaining({ index: 1, name: 'MatMul', cardId: '0', lane: 'stream', startTime: 1000, duration: 50 }),
            expect.objectContaining({ index: 2, startTime: 1500, duration: 100 }),
        ]);
        expect(session.selectedData).toBeUndefined();
        expect(session.selectedUnits).toEqual([]);
        expect(session.domainRange).toEqual(before);
        expect(JSON.stringify(result)).not.toContain('/private/');
        await select({ candidateRef: result.candidates[1].candidateRef });
        expect(session.selectedData?.id).toBe('slice-2');
        expect(session.domainRange).toEqual(before);
    });

    test('returns only overlapping time ranges, including a visible zero-duration operator', async () => {
        mountSource([
            { ...slice, id: 'left', startTime: -100, duration: 50 },
            { ...slice, id: 'right', startTime: 2100 },
            { ...slice, id: 'overlap', startTime: -100, duration: 200 },
            { ...slice, id: 'zero', startTime: 500, duration: 0 },
        ]);
        const result = await select({ name: 'MatMul' });
        expect(result.totalCount).toBe(2);
        expect(result.candidates.map((item: any) => item.startTime)).toEqual([-100, 500]);
    });

    test('excludes off-screen, hidden and clipped depths, including virtual-scroll overscan', async () => {
        mountSource([slice], undefined, window.innerHeight + 10);
        const visible = mountSource([{ ...slice, id: 'visible' }, { ...slice, id: 'clipped', depth: 3 }]);
        const clip = document.createElement('div');
        clip.style.overflow = 'auto';
        clip.getBoundingClientRect = () => rect(100, 30);
        document.body.appendChild(clip);
        clip.appendChild(visible.element);
        stops.push(() => clip.remove());
        const hidden = mountSource([{ ...slice, id: 'hidden' }]);
        hidden.element.style.display = 'none';
        const result = await select({ name: 'MatMul' });
        expect(result).toMatchObject({ status: 'selected', totalCount: 1 });
        expect(session.selectedData?.id).toBe('visible');
    });

    test('deduplicates the same operator rendered in both pinned and normal areas', async () => {
        mountSource();
        mountSource([slice], undefined, 220);
        await expect(select({ name: 'MatMul' })).resolves.toMatchObject({ totalCount: 1, status: 'selected' });
    });

    test('distinguishes same names across cards and supports explicit card filtering', async () => {
        mountSource();
        const other = createThread('t1', 'another stream', '1');
        mountSource([{ ...slice, cardId: '1' }], other, 200);
        const result = await select({ name: 'MatMul' });
        expect(result.candidates.map((item: any) => item.cardId)).toEqual(['0', '1']);
        await expect(select({ name: 'MatMul', cardId: '1' })).resolves.toMatchObject({ status: 'selected', operator: { cardId: '1' } });
    });

    test('supports case-insensitive substring matching using the real rendered names', async () => {
        mountSource([{ ...slice, name: 'aten::Add', type: 'aten::Add' }]);
        await expect(select({ name: 'add' })).rejects.toMatchObject({ code: 'COMMAND_NOT_FOUND' });
        await expect(select({ name: 'add', isMatchCase: false, isMatchExact: false })).resolves.toMatchObject({ operator: { name: 'aten::Add' } });
    });

    test.each(['scroll', 'zoom', 'reload', 'filter', 'unmount'])('rejects old choices after %s', async change => {
        const source = mountSource([slice, { ...slice, id: 'slice-2', startTime: 1500 }]);
        const result = await select({ name: 'MatMul' });
        if (change === 'scroll') source.element.getBoundingClientRect = () => rect(80);
        if (change === 'zoom') session.domainRange = { domainStart: 500, domainEnd: 1500 };
        if (change === 'filter') session.areFlagEventsHidden = !session.areFlagEventsHidden;
        if (change === 'reload') source.ready = false;
        if (change === 'unmount') source.element.remove();
        await expect(select({ candidateRef: result.candidates[0].candidateRef })).rejects.toThrow();
        expect(session.selectedData).toBeUndefined();
    });

    test('does not pick a unique match until all visible sources have finished loading', async () => {
        mountSource();
        const loading = mountSource([], session.units[0].children?.[1] as InsightUnit, 200);
        loading.ready = false;
        await expect(select({ name: 'MatMul' })).rejects.toMatchObject({ code: 'COMMAND_UNAVAILABLE', retryable: true });
        expect(session.selectedData).toBeUndefined();
        loading.ready = true;
        await expect(select({ name: 'MatMul' })).resolves.toMatchObject({ status: 'selected' });
    });

    test('paginates candidates without selecting and keeps existing references stable', async () => {
        mountSource(Array.from({ length: 24 }, (_, index) => ({ ...slice, id: `slice-${index}`, startTime: 100 + index * 50 })));
        const first = await select({ name: 'MatMul', limit: 10 });
        const next = await select({ name: 'MatMul', offset: 10, limit: 10 });
        expect(first).toMatchObject({ needsChoice: true, totalCount: 24, hasMore: true });
        expect(next.candidates[0].index).toBe(11);
        expect(session.selectedData).toBeUndefined();
        await expect(select({ candidateRef: first.candidates[0].candidateRef })).resolves.toMatchObject({ status: 'selected' });
    });

    test.each<JsonObject>([{}, { name: '' }, { name: 'MatMul', index: 1 }, { name: 'MatMul', candidateRef: 'x' }, { name: 'MatMul', limit: 51 }, { name: 'MatMul', offset: -1 }])('rejects invalid arguments %j', async args => {
        await expect(select(args)).rejects.toMatchObject({ code: 'COMMAND_INVALID' });
    });

    test('reports missing rendered lanes and no match distinctly', async () => {
        await expect(select({ name: 'MatMul' })).rejects.toMatchObject({ code: 'COMMAND_UNAVAILABLE' });
        mountSource([]);
        await expect(select({ name: 'MatMul' })).rejects.toMatchObject({ code: 'COMMAND_NOT_FOUND' });
    });

    test('honors selection lock, an in-flight navigation, cancellation and deadline before mutation', async () => {
        mountSource();
        session.selectedRangeIsLock = true;
        await expect(select({ name: 'MatMul' })).rejects.toMatchObject({ code: 'COMMAND_UNAVAILABLE' });
        session.selectedRangeIsLock = false;
        session.locateUnit = { target: () => false, onSuccess: () => {}, showDetail: false };
        await expect(select({ name: 'MatMul' })).rejects.toMatchObject({ code: 'COMMAND_BUSY' });
        session.locateUnit = undefined;
        const abort = new AbortController();
        abort.abort();
        await expect(invoke('Timeline.selectSlice', { name: 'MatMul' }, abort.signal)).rejects.toMatchObject({ code: 'COMMAND_CANCELLED' });
        await expect(invoke('Timeline.selectSlice', { name: 'MatMul' }, undefined, Date.now() - 1)).rejects.toMatchObject({ code: 'COMMAND_TIMEOUT' });
        expect(session.selectedData).toBeUndefined();
    });
});

describe('pin lanes with the same name', () => {
    test('pins matching lanes only and refuses a repeated call after the menu becomes unavailable', () => {
        session.selectedUnits = [session.units[0].children?.[0] as InsightUnit];
        expect(invoke('Timeline.pinByUnitName')).toEqual({ unchanged: false, addedCount: 2, pinnedUnitCount: 2, requiresObserve: true });
        expect(session.pinnedUnits.every(lane => isPinned(lane))).toBe(true);
        expect(observeTimeline()).toMatchObject({ actions: { pinByUnitName: false }, pinnedUnitCount: 2 });
        expect(() => invoke('Timeline.pinByUnitName')).toThrow('not available');
        expect(session.pinnedUnits).toHaveLength(2);
    });

    test('does not duplicate lanes that were already pinned', () => {
        const [first, second] = session.units[0].children as InsightUnit[];
        switchPinned(second);
        session.pinnedUnits = [second];
        session.selectedUnits = [first];
        expect(invoke('Timeline.pinByUnitName')).toMatchObject({ addedCount: 1, pinnedUnitCount: 2 });
    });

    test('retains the context-menu cap of 100 matches', () => {
        jest.spyOn(message, 'warning').mockReturnValue(Object.assign(() => {}, { then: Promise.resolve(true).then.bind(Promise.resolve(true)) }));
        const lanes = Array.from({ length: 101 }, (_, index) => createThread(String(index)));
        session.units = [createCard('0', lanes)];
        session.selectedUnits = [lanes[0]];
        expect(invoke('Timeline.pinByUnitName')).toMatchObject({ addedCount: 100 });
        expect(session.pinnedUnits).toHaveLength(100);
        expect(isPinned(lanes[100])).toBe(false);
    });

    test('preserves legacy group-name matching', () => {
        const first = createThread('a', 'Group A');
        const second = createThread('b', 'Group B');
        (first.metadata as ThreadMetaData).groupNameValue = 'group';
        (second.metadata as ThreadMetaData).groupNameValue = 'group';
        session.units = [createCard('0', [first, second])];
        session.selectedUnits = [first];
        expect(invoke('Timeline.pinByUnitName')).toMatchObject({ addedCount: 2 });
    });

    test('rejects empty, non-leaf and merged lane selections', () => {
        expect(() => invoke('Timeline.pinByUnitName')).toThrow('not available');
        session.selectedUnits = [session.units[0]];
        expect(() => invoke('Timeline.pinByUnitName')).toThrow('not available');
        const lane = session.units[0].children?.[0] as InsightUnit;
        (lane.metadata as ThreadMetaData).threadIdList = ['t1', 't2'];
        session.selectedUnits = [lane];
        expect(() => invoke('Timeline.pinByUnitName')).toThrow('not available');
        expect(session.pinnedUnits).toHaveLength(0);
    });

    test('rejects unknown arguments and inactive sessions', () => {
        expect(() => invoke('Timeline.pinByUnitName', { name: 'stream' })).toThrow('does not accept');
        setTimelineAgentSession(undefined);
        expect(() => invoke('Timeline.pinByUnitName')).toThrow('No active');
    });
});
