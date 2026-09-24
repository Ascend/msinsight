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
import { Session } from '../entity/session';
import type { InsightUnit } from '../entity/insight';
import type { SelectedDataType } from '../entity/session';
import { observeTimeline, registerTimelineCommands, setTimelineAgentSession } from './timelineController';

const MODULE_ID = 'Timeline';
const MAX_TEST_ZOOM_STEPS = 256;

const handlers = new Map<string, CommandHandler>();
const definitions = new Map<string, CommandDefinition>();
const client = {
    registerCommand(definition: CommandDefinition, handler: CommandHandler): () => void {
        definitions.set(definition.name, definition);
        handlers.set(definition.name, handler);
        return () => {
            definitions.delete(definition.name);
            handlers.delete(definition.name);
        };
    },
} as unknown as ModuleAgentCommandClient;

const invoke = (name: string, args: JsonObject = {}): unknown =>
    handlers.get(name)?.(args, {
        requestId: 'request-1',
        deadline: Date.now() + 5000,
        signal: new AbortController().signal,
    });

const createValidSession = (): Session => {
    const session = new Session({ isNsMode: true });
    session.phase = 'download';
    return session;
};

const createPanSession = (): Session => {
    const session = new Session({ isNsMode: false });
    session.phase = 'download';
    session.realTimeUpdate = false;
    session.endTimeAll = 40000;
    session.domainRange = { domainStart: 0, domainEnd: 20000 };
    session.cancelZoomingHistory();
    session.contextMenu.zoomHistory = [];
    return session;
};

let unregister: (() => void) | undefined;

beforeEach(() => {
    handlers.clear();
    definitions.clear();
    unregister = registerTimelineCommands(client);
    setTimelineAgentSession(undefined);
});

afterEach(() => {
    unregister?.();
    unregister = undefined;
    setTimelineAgentSession(undefined);
});

const getObs = (key: string): unknown => (observeTimeline() as Record<string, unknown>)[key];

describe('registration', () => {
    test('registers exactly four Timeline commands', () => {
        expect([...definitions.keys()].sort()).toEqual([
            'Timeline.focus',
            'Timeline.pan',
            'Timeline.restore',
            'Timeline.zoom',
        ]);
    });

    test('each command has a name, title, description, and strict inputSchema', () => {
        for (const [name, def] of definitions) {
            expect(def.name).toBe(name);
            expect(def.title).toBeTruthy();
            expect(def.description).toBeTruthy();
            expect(def.inputSchema).toMatchObject({
                type: 'object',
                additionalProperties: false,
                properties: expect.any(Object),
            });
            if (name === 'Timeline.zoom') {
                expect(def.inputSchema.oneOf).toEqual([{ required: ['direction'] }, { required: ['percentage'] }]);
                expect(def.inputSchema.properties).toHaveProperty('percentage', expect.objectContaining({ minimum: 1, maximum: 90 }));
            } else {
                expect(def.inputSchema.required).toEqual([expect.any(String)]);
            }
        }
    });
});

describe('observation', () => {
    test('returns available: false when no session is active', () => {
        setTimelineAgentSession(undefined);
        const obs = observeTimeline() as Record<string, unknown>;
        expect(obs.moduleId).toBe(MODULE_ID);
        expect(obs.available).toBe(false);
    });

    test('returns available: false when session is configuring', () => {
        const session = new Session({ isNsMode: true });
        setTimelineAgentSession(session);
        const obs = observeTimeline() as Record<string, unknown>;
        expect(obs.available).toBe(false);
        expect(obs.phase).toBe('configuring');
    });

    test('returns compact observation for a valid session', () => {
        const session = createValidSession();
        setTimelineAgentSession(session);
        const obs = observeTimeline() as Record<string, unknown>;

        expect(obs).toMatchObject({
            moduleId: MODULE_ID,
            available: true,
            phase: 'download',
            timeUnit: 'ns',
        });
        expect(obs.observedAt).toBeGreaterThan(0);
        expect(obs.viewport).toMatchObject({ start: 0, end: 2e10, duration: 2e10 });
        expect(obs.selection).toEqual({
            range: null,
            operator: null,
            units: { count: 0, items: [], truncated: false },
        });
    });

    test('does not leak dbPath, rawStartTime, color, or index-signature fields', () => {
        const session = createValidSession();
        session.selectedData = {
            name: 'op1',
            startTime: 1000,
            duration: 500,
            threadId: 't1',
            processId: 'p1',
            cardId: 'c1',
            metaType: 'TEXT',
            dbPath: '/secret/path',
            rawStartTime: 'hidden',
            color: [[0, 'red' as any]],
            unknownInject: 'should not appear',
        } as unknown as SelectedDataType;

        setTimelineAgentSession(session);
        const operator = getObs('selection') as Record<string, unknown>;
        expect(operator.operator).toEqual({
            name: 'op1',
            startTime: 1000,
            duration: 500,
            threadId: 't1',
            processId: 'p1',
            cardId: 'c1',
            metaType: 'TEXT',
        });
    });

    test('truncates selectedUnits beyond the limit and does not serialize InsightUnit objects', () => {
        const session = createValidSession();
        const units: InsightUnit[] = Array.from({ length: 15 }, (_, i) => ({
            name: `unit-${i}`,
            isExpanded: false,
            isDisplay: true,
            isUnitVisible: true,
            isMultiDeviceHidden: false,
            isMerged: false,
        }) as unknown as InsightUnit);
        session.selectedUnits = units;

        setTimelineAgentSession(session);
        const sel = getObs('selection') as Record<string, unknown>;
        const unitsSummary = sel.units as Record<string, unknown>;
        const items = unitsSummary.items as Record<string, unknown>[];

        expect(unitsSummary.count).toBe(15);
        expect(items).toHaveLength(10);
        expect(unitsSummary.truncated).toBe(true);
        items.forEach(item => {
            expect(Object.keys(item)).toEqual(['name', 'expanded', 'visible']);
        });
    });

    test('reports correct timeUnit and dataComposition', () => {
        const msSession = new Session({ isNsMode: false });
        msSession.phase = 'download';
        setTimelineAgentSession(msSession);
        expect(getObs('timeUnit')).toBe('ms');

        const nsSession = createValidSession();
        setTimelineAgentSession(nsSession);
        expect(getObs('timeUnit')).toBe('ns');

        nsSession.hasFtraceData = true;
        const comp = getObs('dataComposition') as Record<string, unknown>;
        expect(comp.hasFtraceData).toBe(true);
        expect(comp.hasNonFtraceData).toBe(false);
    });

    test('reports available navigation modes based on current session state', () => {
        const session = createValidSession();
        setTimelineAgentSession(session);

        const nav = getObs('navigation') as Record<string, unknown[]>;
        expect(nav.zoom).toEqual(['in']);
        expect(nav.pan).toEqual([]);
        expect(nav.restore).toEqual([]);
        expect(nav.focus).toEqual([]);

        session.selectedData = { name: 'op', startTime: 0, duration: 100, threadId: 't1', processId: 'p1' } as SelectedDataType;
        expect((getObs('navigation') as Record<string, unknown[]>).focus).toEqual(['operator']);

        session.selectedRange = [100, 200] as [number, number];
        expect((getObs('navigation') as Record<string, unknown[]>).focus).toEqual(['operator', 'selection']);
    });
});

describe('zoom command', () => {
    test('zooms in reducing the viewport duration', () => {
        const session = createValidSession();
        setTimelineAgentSession(session);
        const before = session.domainRange;
        const result = invoke('Timeline.zoom', { direction: 'in' }) as Record<string, unknown>;

        expect(result.unchanged).toBe(false);
        const vp = result.viewport as Record<string, number>;
        expect(vp.end - vp.start).toBeLessThan(before.domainEnd - before.domainStart);
    });

    test('returns unchanged when zoom in is at lower bound', () => {
        const session = createValidSession();
        session.domainRange = { domainStart: 0, domainEnd: 1 };
        setTimelineAgentSession(session);

        expect((invoke('Timeline.zoom', { direction: 'in' }) as Record<string, unknown>).unchanged).toBe(true);
    });

    test('returns unchanged when zoom out is at upper bound', () => {
        const session = createValidSession();
        setTimelineAgentSession(session);

        expect((invoke('Timeline.zoom', { direction: 'out' }) as Record<string, unknown>).unchanged).toBe(true);
    });

    test('zooms out when maxDuration allows', () => {
        const session = createValidSession();
        session.realTimeUpdate = false;
        session.endTimeAll = 4e10;

        const before = session.domainRange;
        setTimelineAgentSession(session);

        const result = invoke('Timeline.zoom', { direction: 'out' }) as Record<string, unknown>;
        expect(result.unchanged).toBe(false);
        const vp = result.viewport as Record<string, number>;
        expect(vp.end - vp.start).toBeGreaterThan(before.domainEnd - before.domainStart);
    });

    test('commits zoom history before returning', () => {
        const session = createValidSession();
        setTimelineAgentSession(session);

        invoke('Timeline.zoom', { direction: 'in' });

        expect(session.contextMenu.zoomHistory).toHaveLength(1);
        expect((getObs('navigation') as Record<string, unknown[]>).restore).toEqual(['undo', 'reset']);
    });
});

describe('percentage zoom', () => {
    test.each([1, 10, 90])('zooms near %s percent with a bounded synchronous W/S burst', percentage => {
        const session = createPanSession();
        const initial = session.domainRange;
        setTimelineAgentSession(session);
        const result = invoke('Timeline.zoom', { percentage }) as JsonObject;
        const target = Math.max(session.domain.lowerBound, 40000 * percentage / 100);

        expect(Math.abs(session.domain.duration - target)).toBeLessThan(target * 0.07);
        expect(result.requestedPercentage).toBe(percentage);
        expect(result.actualPercentage).toBeCloseTo(session.domain.duration / 40000 * 100);
        if (percentage === 1) {
            expect(result.actualPercentage).toBeCloseTo(session.domain.lowerBound / 40000 * 100);
        } else {
            expect(result.actualPercentage).toBeCloseTo(percentage, 0);
        }
        expect(result.globalDuration).toBe(40000);
        expect(result.timeUnit).toBe('ms');
        expect(result.completedSteps).toBeGreaterThan(0);
        expect(result.limitedByBounds).toBe(percentage === 1);
        // This fixture starts at a non-default viewport without history: save the base plus final range.
        expect(session.contextMenu.zoomHistory).toHaveLength(2);
        invoke('Timeline.restore', { mode: 'undo' });
        expect(session.domainRange).toEqual(initial);
    });

    test('uses the same global percentage in nanosecond mode', () => {
        const session = createValidSession();
        session.realTimeUpdate = false;
        session.endTimeAll = 2e10;
        setTimelineAgentSession(session);
        const result = invoke('Timeline.zoom', { percentage: 1 }) as JsonObject;

        expect(result.actualPercentage).toBeCloseTo(1, 0);
        expect(result.timeUnit).toBe('ns');
        expect(result.globalDuration).toBe(2e10);
        expect(result.limitedByBounds).toBe(false);
        expect(session.contextMenu.zoomHistory).toHaveLength(1);
    });

    test('applies hundreds of equivalent W/S steps with one Domain update', () => {
        const session = createValidSession();
        session.realTimeUpdate = false;
        session.endTimeAll = Number.MAX_VALUE;
        let zoomAssignments = 0;
        const descriptor = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(session.domain), 'zoom');
        expect(descriptor?.set).toBeDefined();
        Object.defineProperty(session.domain, 'zoom', {
            configurable: true,
            set(value) {
                zoomAssignments++;
                descriptor?.set?.call(session.domain, value);
            },
        });
        setTimelineAgentSession(session);

        const result = invoke('Timeline.zoom', { percentage: 1 }) as JsonObject;

        expect(result.completedSteps).toBe(MAX_TEST_ZOOM_STEPS);
        expect(result.stepLimitReached).toBe(true);
        expect(zoomAssignments).toBe(1);
    });

    test('does not add steps or history when already near the target', () => {
        const session = createPanSession();
        setTimelineAgentSession(session);
        const result = invoke('Timeline.zoom', { percentage: 50 }) as JsonObject;

        expect(result.completedSteps).toBe(0);
        expect(result.unchanged).toBe(true);
        expect(session.contextMenu.zoomHistory).toHaveLength(0);
    });

    test('rejects percentage zoom when the full duration is unavailable', () => {
        const session = createValidSession();
        setTimelineAgentSession(session);
        expect(() => invoke('Timeline.zoom', { percentage: 10 })).toThrow('full Timeline duration');
    });

    test.each([0, 91, -1, NaN, Infinity, '10'])('rejects invalid percentage %s', percentage => {
        expect(() => invoke('Timeline.zoom', { percentage })).toThrow();
    });

    test('rejects mixed modes and unknown arguments', () => {
        expect(() => invoke('Timeline.zoom', { percentage: 10, direction: 'in' })).toThrow();
        expect(() => invoke('Timeline.zoom', { percentage: 10, extra: true })).toThrow();
    });
});

describe('pan command', () => {
    test('pans right when direction is right', () => {
        const session = createPanSession();
        setTimelineAgentSession(session);

        const result = invoke('Timeline.pan', { direction: 'right' }) as Record<string, unknown>;
        expect(result.unchanged).toBe(false);
        expect((result.viewport as Record<string, number>).start).toBeGreaterThan(0);
    });

    test('pans left when domain is not at zero', () => {
        const session = createPanSession();
        session.domainRange = { domainStart: 5000, domainEnd: 25000 };
        setTimelineAgentSession(session);

        const result = invoke('Timeline.pan', { direction: 'left' }) as Record<string, unknown>;
        expect(result.unchanged).toBe(false);
        expect((result.viewport as Record<string, number>).start).toBeLessThan(5000);
    });

    test('returns unchanged without recording history when pan left is at the start boundary', () => {
        const session = createPanSession();
        setTimelineAgentSession(session);

        expect((invoke('Timeline.pan', { direction: 'left' }) as Record<string, unknown>).unchanged).toBe(true);
        expect(session.contextMenu.zoomHistory).toHaveLength(0);
    });
});

describe('restore command', () => {
    test('undo with no zoom history returns unchanged', () => {
        const session = createValidSession();
        setTimelineAgentSession(session);

        expect((invoke('Timeline.restore', { mode: 'undo' }) as Record<string, unknown>).unchanged).toBe(true);
    });

    test('undo immediately restores the previous viewport after zoom', () => {
        const session = createValidSession();
        const initial = session.domainRange;
        setTimelineAgentSession(session);

        invoke('Timeline.zoom', { direction: 'in' });
        const result = invoke('Timeline.restore', { mode: 'undo' }) as Record<string, unknown>;

        expect(result.unchanged).toBe(false);
        expect(result.viewport).toEqual({
            start: initial.domainStart,
            end: initial.domainEnd,
            duration: initial.domainEnd - initial.domainStart,
        });
        expect(session.contextMenu.zoomHistory).toHaveLength(0);
    });

    test('reset with no zoom history returns unchanged', () => {
        const session = createValidSession();
        setTimelineAgentSession(session);

        expect((invoke('Timeline.restore', { mode: 'reset' }) as Record<string, unknown>).unchanged).toBe(true);
    });

    test('reset restores the domain to default after zoom history', () => {
        const session = createValidSession();
        jest.useFakeTimers();
        session.domainRange = { domainStart: 0, domainEnd: 2e10 };
        jest.advanceTimersByTime(500);
        session.domainRange = { domainStart: 5e9, domainEnd: 15e9 };
        jest.advanceTimersByTime(500);
        expect(session.contextMenu.zoomHistory.length).toBeGreaterThanOrEqual(1);
        setTimelineAgentSession(session);

        const result = invoke('Timeline.restore', { mode: 'reset' }) as Record<string, unknown>;
        expect(result.unchanged).toBe(false);
        jest.useRealTimers();
    });
});

describe('focus command', () => {
    test('focus on operator with selected data returns unchanged false', () => {
        const session = createValidSession();
        session.selectedData = { name: 'op', startTime: 1000, duration: 500, threadId: 't1', processId: 'p1' } as SelectedDataType;
        setTimelineAgentSession(session);

        expect((invoke('Timeline.focus', { target: 'operator' }) as Record<string, unknown>).unchanged).toBe(false);
    });

    test('focus on selection with selected range returns unchanged false', () => {
        const session = createValidSession();
        session.selectedRange = [5e9, 7e9] as [number, number];
        session.selectedData = { name: 'op', startTime: 5e9, duration: 2e9, threadId: 't1', processId: 'p1' } as SelectedDataType;
        setTimelineAgentSession(session);

        const result = invoke('Timeline.focus', { target: 'selection' }) as Record<string, unknown>;
        expect(result.unchanged).toBe(false);
        const vp = result.viewport as Record<string, number>;
        expect(vp.start).toBe(5e9);
        expect(vp.end).toBe(7e9);
    });

    test('focus returns unchanged when the target is not available', () => {
        const session = createValidSession();
        setTimelineAgentSession(session);

        expect((invoke('Timeline.focus', { target: 'operator' }) as Record<string, unknown>).unchanged).toBe(true);
        expect((invoke('Timeline.focus', { target: 'selection' }) as Record<string, unknown>).unchanged).toBe(true);
    });

    test('does not expose or focus a zero-duration operator', () => {
        const session = createValidSession();
        session.selectedData = {
            name: 'instant',
            startTime: 1000,
            duration: 0,
            threadId: 't1',
            processId: 'p1',
        } as SelectedDataType;
        const before = session.domainRange;
        setTimelineAgentSession(session);

        expect((getObs('navigation') as Record<string, unknown[]>).focus).toEqual([]);
        expect(invoke('Timeline.focus', { target: 'operator' })).toEqual({
            unchanged: true,
            viewport: {
                start: before.domainStart,
                end: before.domainEnd,
                duration: before.domainEnd - before.domainStart,
            },
            requiresObserve: true,
        });
    });

    test('does not expose or focus a zero-width selection', () => {
        const session = createValidSession();
        session.selectedRange = [1000, 1000];
        const before = session.domainRange;
        setTimelineAgentSession(session);

        expect((getObs('navigation') as Record<string, unknown[]>).focus).toEqual([]);
        expect(invoke('Timeline.focus', { target: 'selection' })).toEqual({
            unchanged: true,
            viewport: {
                start: before.domainStart,
                end: before.domainEnd,
                duration: before.domainEnd - before.domainStart,
            },
            requiresObserve: true,
        });
    });
});

describe('command validation', () => {
    test('rejects an invalid enum value', () => {
        expect(() => invoke('Timeline.zoom', { direction: 'sideways' })).toThrow();
        expect(() => invoke('Timeline.pan', { direction: 'up' })).toThrow();
        expect(() => invoke('Timeline.restore', { mode: 'forward' })).toThrow();
        expect(() => invoke('Timeline.focus', { target: 'all' })).toThrow();
    });

    test('rejects missing required argument', () => {
        expect(() => invoke('Timeline.zoom', {})).toThrow();
        expect(() => invoke('Timeline.pan', {})).toThrow();
        expect(() => invoke('Timeline.restore', {})).toThrow();
        expect(() => invoke('Timeline.focus', {})).toThrow();
    });

    test('rejects extra properties', () => {
        expect(() => invoke('Timeline.zoom', { direction: 'in', extra: true })).toThrow();
    });
});

describe('all commands return requiresObserve: true and a viewport', () => {
    test.each(['Timeline.zoom', 'Timeline.pan', 'Timeline.restore', 'Timeline.focus'] as const)(
        '%s returns a result with viewport and requiresObserve',
        (name) => {
            const session = createValidSession();
            session.selectedData = { name: 'op', startTime: 0, duration: 500, threadId: 't1', processId: 'p1' } as SelectedDataType;
            session.selectedRange = [0, 500] as [number, number];
            setTimelineAgentSession(session);

            // Annotate JsonObject explicitly: the ternary chain infers a union with optional `undefined`
            // properties, which the JsonObject index signature rejects.
            const input: JsonObject = name === 'Timeline.zoom' ? { direction: 'in' }
                : name === 'Timeline.pan' ? { direction: 'right' }
                    : name === 'Timeline.restore' ? { mode: 'undo' }
                        : { target: 'operator' };
            const result = invoke(name, input) as Record<string, unknown>;
            expect(result).toHaveProperty('viewport');
            expect(result).toHaveProperty('requiresObserve', true);
            expect(result).toHaveProperty('unchanged');
        },
    );
});
