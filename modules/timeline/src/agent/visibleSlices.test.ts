/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { CommandDefinition, CommandHandler, JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { unit, type InsightUnit } from '../entity/insight';
import type { CardMetaData, ThreadMetaData } from '../entity/data';
import type { StackStatusData } from '../entity/chart';
import { Session } from '../entity/session';
import { registerVisibleSliceSource, visibleSliceScope, type VisibleSliceSource } from './visibleSlices';
import { observeTimeline, registerTimelineCommands, setTimelineAgentSession } from './timelineController';

jest.mock('../components/detailViews/Common', () => ({ searchAllSlices: jest.fn() }));
jest.mock('../components/CategorySearch', () => ({ calculateDomainRange: jest.fn() }));
jest.mock('../insight/units/AscendUnit', () => ({ ThreadUnit: class { metadata = {}; } }));

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
const list = (args: JsonObject = {}): any => invoke('Timeline.listVisibleSlices', args);
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
        select: jest.fn(),
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

describe('list visible slices', () => {
    test('returns empty serializable pages and summary-only observation', () => {
        const result = list();
        expect(result).toMatchObject({ totalCount: 0, loading: false, sourceCount: 0, offset: 0, limit: 20, hasMore: false, timeUnit: 'ns', slices: [] });
        expect(observeTimeline()).toMatchObject({ visibleSlices: { count: 0, loading: false, sourceCount: 0, revision: result.revision }, selection: { operator: null } });
        expect(Object.keys(observeTimeline().visibleSlices as JsonObject).sort()).toEqual(['count', 'loading', 'revision', 'sourceCount']);
    });

    test('paginates bounded summaries without selecting or disclosing private paths', () => {
        const source = mountSource(Array.from({ length: 65 }, (_, index) => ({ ...slice, id: `slice-${index}`, startTime: 100 + index })));
        const before = { ...session.domainRange };
        const first = list();
        expect(first.slices).toHaveLength(20);
        expect(first).toMatchObject({ totalCount: 65, hasMore: true });
        const next = list({ offset: 20, limit: 50, revision: first.revision });
        expect(next.slices).toHaveLength(45);
        expect(next.hasMore).toBe(false);
        expect(next.slices[0]).toMatchObject({ id: 'slice-20', cardId: '0', processId: 'p1', threadId: 't1', metaType: 'TEXT', lane: 'stream', name: 'MatMul' });
        expect(next.slices[0].sourceRef).toBe(first.slices[0].sourceRef);
        expect(list({ offset: 100, revision: first.revision }).slices).toEqual([]);
        expect(JSON.stringify(next)).not.toContain('/private/');
        expect(JSON.stringify(next)).not.toContain('originalStartTime');
        expect(session.domainRange).toEqual(before);
        expect(session.selectedData).toBeUndefined();
        expect(source.select).not.toHaveBeenCalled();
        expect(window.request).not.toHaveBeenCalled();
    });

    test.each<JsonObject>([{ offset: -1 }, { limit: 51 }, { offset: 1 }])('rejects invalid arguments %j', args => {
        expect(() => list(args)).toThrow(expect.objectContaining({ code: 'COMMAND_INVALID' }));
    });

    test('deduplicates pinned copies but distinguishes database sources', () => {
        const source = mountSource();
        mountSource([slice], source.unit, 150);
        const other = createThread('t1');
        (other.metadata as ThreadMetaData).dbPath = '/private/other.db';
        mountSource([slice], other, 200);
        const result = list();
        expect(result).toMatchObject({ totalCount: 2, sourceCount: 3 });
        expect(new Set(result.slices.map((item: JsonObject) => item.sourceRef)).size).toBe(2);
    });

    test('excludes offscreen and unready data, marking partial results as loading', () => {
        mountSource([{ ...slice, id: 'outside', startTime: 3000 }, { ...slice, id: 'deep', depth: 10 }, slice]);
        const pending = mountSource([{ ...slice, id: 'pending' }]);
        pending.ready = false;
        expect(list()).toMatchObject({ totalCount: 1, loading: true, sourceCount: 2 });
        expect(observeTimeline()).toMatchObject({ visibleSlices: { count: 1, loading: true } });
    });

    test.each(['viewport', 'data'] as const)('invalidates revision on %s changes', change => {
        const source = mountSource();
        const first = list();
        expect(list().revision).toBe(first.revision);
        if (change === 'viewport') session.domainRange = { domainStart: 100, domainEnd: 1500 };
        if (change === 'data') source.data = [[{ ...slice, id: 'replacement' }]];
        expect(() => list({ offset: 1, revision: first.revision })).toThrow(expect.objectContaining({ code: 'COMMAND_INVALID' }));
        expect(list().revision).not.toBe(first.revision);
        expect(session.selectedData).toBeUndefined();
        expect(source.select).not.toHaveBeenCalled();
    });

    test('honors cancellation, deadline and unavailable sessions', () => {
        const abort = new AbortController();
        abort.abort();
        expect(() => invoke('Timeline.listVisibleSlices', {}, abort.signal)).toThrow(expect.objectContaining({ code: 'COMMAND_CANCELLED' }));
        expect(() => invoke('Timeline.listVisibleSlices', {}, undefined, Date.now() - 1)).toThrow(expect.objectContaining({ code: 'COMMAND_TIMEOUT' }));
        setTimelineAgentSession(undefined);
        expect(() => list()).toThrow(expect.objectContaining({ code: 'COMMAND_UNAVAILABLE' }));
        expect(observeTimeline()).not.toHaveProperty('visibleSlices');
    });
});
