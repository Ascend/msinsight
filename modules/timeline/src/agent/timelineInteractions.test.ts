/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { calculateDomainRange, searchCategory } from '../components/CategorySearch';
import { ThreadUnit } from '../insight/units/AscendUnit';
import { message } from 'antd';
import type { CommandDefinition, CommandHandler, JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { unit, type InsightUnit } from '../entity/insight';
import type { CardMetaData, ThreadMetaData } from '../entity/data';
import type { StackStatusData } from '../entity/chart';
import { Session } from '../entity/session';
import { store } from '../store';
import { isPinned, switchPinned } from '../components/ChartContainer/unitPin';
import { selectRenderedSlice } from '../utils/selectRenderedSlice';
import { observeTimeline, registerTimelineCommands, setTimelineAgentSession } from './timelineController';
import { registerVisibleSliceSource, visibleSliceScope } from './visibleSlices';

jest.mock('../store', () => ({ store: { sessionStore: { activeSession: undefined } } }));
jest.mock('../components/CategorySearch', () => ({ calculateDomainRange: jest.fn(() => [80, 140]), searchCategory: jest.fn() }));
jest.mock('../insight/units/AscendUnit', () => ({
    ThreadUnit: jest.requireActual('../entity/insight').unit({ name: 'Thread' }),
}));

const Thread = ThreadUnit;
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
beforeEach(() => {
    session = new Session({ isNsMode: true });
    store.sessionStore.activeSession = session;
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
    jest.mocked(calculateDomainRange).mockReturnValue([80, 140]);
});

afterEach(() => {
    stops.forEach(stop => stop());
    setTimelineAgentSession(undefined);
    store.sessionStore.activeSession = undefined;
    session.cancelZoomingHistory();
    window.request = originalRequest;
    jest.restoreAllMocks();
});

describe('select slice through CategorySearch', () => {
    beforeEach(() => { jest.mocked(searchCategory).mockReset(); });

    test('selects a visible slice through its click callback without searching or changing the viewport', async () => {
        const lane = session.units[0].children?.[0] as InsightUnit;
        const element = document.createElement('div');
        element.getBoundingClientRect = () => ({ top: 100, bottom: 180, left: 200, right: 1000, width: 800, height: 80 } as DOMRect);
        document.body.appendChild(element);
        const onClick = jest.fn();
        const unregister = registerVisibleSliceSource(session, {
            unit: lane,
            element,
            data: [[slice]],
            ...session.domainRange,
            rowHeight: 20,
            ready: true,
            scope: visibleSliceScope(session, lane),
            select: target => selectRenderedSlice(session, lane.metadata as ThreadMetaData, target, onClick, lane),
        });
        stops.push(() => { unregister(); element.remove(); });
        const viewport = { ...session.domainRange };
        const searchData = session.searchData;
        await expect(select({ name: 'matmul', isMatchExact: true })).resolves.toMatchObject({ status: 'selected' });
        expect(session.selectedData?.id).toBe(slice.id);
        expect(session.domainRange).toEqual(viewport);
        expect(session.searchData).toBe(searchData);
        expect(session.locateUnit).toBeUndefined();
        expect(onClick).toHaveBeenCalledTimes(1);
        expect(searchCategory).not.toHaveBeenCalled();
        expect(window.request).not.toHaveBeenCalled();
    });

    test('delegates first-match selection and loads links after the shared navigation succeeds', async () => {
        const lane = session.units[0].children?.[0] as InsightUnit;
        const onClick = jest.fn();
        lane.chart = { onClick } as unknown as typeof lane.chart;
        jest.mocked(searchCategory).mockImplementation(async (_session, _criteria, options) => {
            selectRenderedSlice(session, lane.metadata as ThreadMetaData, { ...slice }, undefined, lane);
            options.onSuccess?.(lane);
            return 3;
        });
        await expect(select({ name: ' MatMul ' })).resolves.toMatchObject({ status: 'selected', totalCount: 3, requiresObserve: true });
        expect(searchCategory).toHaveBeenCalledWith(session, { content: 'MatMul', rankId: 'ALL', isMatchCase: false, isMatchExact: false }, expect.anything());
        expect(onClick).toHaveBeenCalledWith(session.selectedData, session, lane.metadata);
        expect(window.request).not.toHaveBeenCalled();
    });

    test('forwards card and match options and reports no match', async () => {
        jest.mocked(searchCategory).mockResolvedValue(0);
        await expect(select({ name: 'add', cardId: '0', isMatchCase: true, isMatchExact: true })).rejects.toMatchObject({ code: 'COMMAND_NOT_FOUND' });
        expect(searchCategory).toHaveBeenCalledWith(session, { content: 'add', rankId: '0', isMatchCase: true, isMatchExact: true }, expect.anything());
    });

    test('reports failed navigation rather than stale selection', async () => {
        jest.mocked(searchCategory).mockImplementation(async (_session, _criteria, options) => { options.onFailure(); return 1; });
        await expect(select({ name: 'MatMul' })).rejects.toMatchObject({ code: 'COMMAND_UNAVAILABLE' });
    });

    test('invalidates delegated callbacks after cancellation', async () => {
        let options: Parameters<typeof searchCategory>[2] | undefined;
        jest.mocked(searchCategory).mockImplementation((_session, _criteria, value) => { options = value; return new Promise(() => {}); });
        const abort = new AbortController();
        const pending = invoke('Timeline.selectSlice', { name: 'MatMul' }, abort.signal);
        abort.abort();
        await expect(pending).rejects.toMatchObject({ code: 'COMMAND_CANCELLED' });
        expect(options?.isCurrent()).toBe(false);
        expect(session.selectedData).toBeUndefined();
    });

    test.each<JsonObject>([{}, { name: '' }, { name: 'MatMul', index: 1 }, { candidateRef: 'x' }, { name: 'MatMul', offset: 1 }])('rejects invalid arguments %j', async args => {
        await expect(select(args)).rejects.toMatchObject({ code: 'COMMAND_INVALID' });
        expect(searchCategory).not.toHaveBeenCalled();
    });

    test('honors lock, concurrent navigation and deadline before delegation', async () => {
        session.selectedRangeIsLock = true;
        await expect(select({ name: 'MatMul' })).rejects.toMatchObject({ code: 'COMMAND_UNAVAILABLE' });
        session.selectedRangeIsLock = false;
        session.locateUnit = { target: () => false, onSuccess: () => {} };
        await expect(select({ name: 'MatMul' })).rejects.toMatchObject({ code: 'COMMAND_BUSY' });
        session.locateUnit = undefined;
        await expect(invoke('Timeline.selectSlice', { name: 'MatMul' }, undefined, Date.now() - 1)).rejects.toMatchObject({ code: 'COMMAND_TIMEOUT' });
        expect(searchCategory).not.toHaveBeenCalled();
    });
});

describe('shared rendered selection keeps TARGET context', () => {
    test.each(['host', 'device'] as const)('uses the task thread and owning %s lane before the click callback', side => {
        const lane = session.units[0].children?.[0] as InsightUnit;
        const metadata = lane.metadata as ThreadMetaData;
        metadata.threadId = '';
        metadata.metaType = side === 'device' ? 'Ascend Hardware' : 'TEXT';
        const selected: StackStatusData = { ...slice, threadId: '42' };
        // A range-selected lane must not override the actual slice's source.
        session.selectedUnits = [session.units[0].children?.[1] as InsightUnit];
        const onClick = jest.fn(() => {
            expect(session.selectedDataUnit).toBe(lane);
            expect(session.selectedData).toMatchObject({ threadId: '42', offsetSide: side });
        });

        selectRenderedSlice(session, metadata, selected, onClick, lane);

        expect(session.selectedData).toMatchObject({ id: slice.id, threadId: '42', timestamp: 1100, offsetSide: side });
        expect(session.selectedDataUnit).toBe(lane);
        expect(selected.showSelectedData).toBe(true);
        expect(onClick).toHaveBeenCalledTimes(1);
    });

    test('clears both selected data and its owning lane on an empty click', () => {
        const lane = session.units[0].children?.[0] as InsightUnit;
        const metadata = lane.metadata as ThreadMetaData;
        selectRenderedSlice(session, metadata, { ...slice }, undefined, lane);
        expect(session.selectedDataUnit).toBe(lane);

        selectRenderedSlice(session, metadata, undefined, undefined, lane);

        expect(session.selectedData).toBeUndefined();
        expect(session.selectedDataUnit).toBeUndefined();
        expect(session.selectedRangeData).toBeUndefined();
        expect(session.drawLineMode).toBe('all');
    });

    test('does not mutate the slice or selection while the selection is locked', () => {
        const lane = session.units[0].children?.[0] as InsightUnit;
        const selected = { ...slice, showSelectedData: false };
        session.selectedRangeIsLock = true;
        const onClick = jest.fn();

        selectRenderedSlice(session, lane.metadata as ThreadMetaData, selected, onClick, lane);

        expect(session.selectedData).toBeUndefined();
        expect(session.selectedDataUnit).toBeUndefined();
        expect(selected.showSelectedData).toBe(false);
        expect(onClick).not.toHaveBeenCalled();
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
