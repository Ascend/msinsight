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
import type { CommandDefinition, CommandHandler, JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { Session } from '@/entity/session';
import {
    notifyMemScopeInteractionsChanged,
    observeMemScopeInteractions,
    registerBottomDrawerController,
    registerMemScopeInteractionCommands,
    setMemScopeInteractionSession,
} from './interactionController';

const handlers = new Map<string, CommandHandler>();
const definitions = new Map<string, CommandDefinition>();
let unregisterCommands: (() => void) | undefined;
let unregisterDrawer: (() => void) | undefined;
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

const invoke = (name: string, args: JsonObject): unknown => handlers.get(name)?.(args, {
    requestId: 'request-1',
    deadline: Date.now() + 1000,
    signal: new AbortController().signal,
});

const createSession = (): Session => {
    const session = new Session();
    session.deviceIds = { 'device-0': ['ACL', 'HOST_PINNED'], 'device-1': ['ACL'] };
    session.deviceIdOpts = [{ label: 'device-0', value: 'device-0' }, { label: 'device-1', value: 'device-1' }];
    session.deviceId = 'device-0';
    session.typeOpts = [{ label: 'ACL', value: 'ACL' }, { label: 'HOST_PINNED', value: 'HOST_PINNED' }];
    session.eventType = 'ACL';
    session.threadIds = [101, 202];
    session.threadOps = [{ label: 101, value: 101 }, { label: 202, value: 202 }];
    session.threadId = 101;
    session.funcOptions = [
        { label: 'forward', value: 'forward' },
        { label: 'backward', value: 'backward' },
        { label: 'optimizer.step', value: 'optimizer.step' },
    ];
    session.searchFunc = ['forward'];
    return session;
};

const registerDrawer = (initialOpen: boolean, initialTab: 'sliceDetail' | 'systemView'): void => {
    let open = initialOpen;
    let tab: 'sliceDetail' | 'systemView' = initialTab;
    unregisterDrawer?.();
    unregisterDrawer = registerBottomDrawerController({
        observe: () => ({ open, tab }),
        setOpen: value => { open = value; },
        selectTab: value => { tab = value; },
    });
};

beforeEach(() => {
    handlers.clear();
    definitions.clear();
    unregisterCommands = registerMemScopeInteractionCommands(client);
});

afterEach(() => {
    unregisterCommands?.();
    unregisterCommands = undefined;
    unregisterDrawer?.();
    unregisterDrawer = undefined;
    setMemScopeInteractionSession(undefined);
});

test('publishes only the resident getOptions command before any UI mounts', () => {
    expect([...definitions.keys()]).toEqual(['MemScope.context.getOptions']);
});

test('publishes drawer commands whenever the drawer is mounted, even collapsed', () => {
    registerDrawer(false, 'sliceDetail');

    expect([...definitions.keys()]).toEqual(expect.arrayContaining([
        'MemScope.bottomDrawer.setOpen',
        'MemScope.bottomDrawer.selectTab',
    ]));
    expect(definitions.has('MemScope.systemTable.selectView')).toBe(false);
});

test('publishes selectView only while the drawer is open on the System View tab', () => {
    const session = createSession();
    setMemScopeInteractionSession(session);

    registerDrawer(false, 'systemView');
    expect(definitions.has('MemScope.systemTable.selectView')).toBe(false);

    registerDrawer(true, 'sliceDetail');
    expect(definitions.has('MemScope.systemTable.selectView')).toBe(false);

    registerDrawer(true, 'systemView');
    expect(definitions.has('MemScope.systemTable.selectView')).toBe(true);
    expect(() => invoke('MemScope.systemTable.selectView', { view: 'events' })).not.toThrow();
});

test('withdraws drawer commands after the UI controller unmounts', () => {
    registerDrawer(false, 'sliceDetail');
    unregisterDrawer?.();
    unregisterDrawer = undefined;

    expect(definitions.has('MemScope.bottomDrawer.setOpen')).toBe(false);
    expect(definitions.has('MemScope.bottomDrawer.selectTab')).toBe(false);
});

test('publishes context commands per control once their options arrive', () => {
    const session = createSession();
    session.threadOps = [];
    session.funcOptions = [];
    setMemScopeInteractionSession(session);

    expect([...definitions.keys()]).toEqual(expect.arrayContaining([
        'MemScope.context.getOptions',
        'MemScope.context.selectDevice',
        'MemScope.context.selectEventType',
    ]));
    expect(definitions.has('MemScope.context.selectThread')).toBe(false);
    expect(definitions.has('MemScope.context.setFunctionFilter')).toBe(false);

    session.threadOps = [{ label: 101, value: 101 }];
    session.funcOptions = [{ label: 'forward', value: 'forward' }];
    notifyMemScopeInteractionsChanged();
    expect(definitions.has('MemScope.context.selectThread')).toBe(true);
    expect(definitions.has('MemScope.context.setFunctionFilter')).toBe(true);
});

test('getOptions stays resident but still requires an active session', () => {
    expect(() => invoke('MemScope.context.getOptions', { control: 'device' })).toThrow(/no active analysis session/);
});

test('selectView rejects when the System View tab is not open and active', () => {
    const session = createSession();
    setMemScopeInteractionSession(session);
    registerDrawer(true, 'systemView');
    const selectView = handlers.get('MemScope.systemTable.selectView')!;

    registerDrawer(false, 'sliceDetail');
    expect(() => selectView({ view: 'events' }, {
        requestId: 'request-1',
        deadline: Date.now() + 1000,
        signal: new AbortController().signal,
    })).toThrow(/System View tab must be open/);
});

test('drawer operations set explicit states without toggling', () => {
    registerDrawer(false, 'sliceDetail');

    expect(invoke('MemScope.bottomDrawer.setOpen', { open: true })).toEqual({ open: true });
    expect(invoke('MemScope.bottomDrawer.setOpen', { open: true })).toEqual({ open: true });
    expect(invoke('MemScope.bottomDrawer.selectTab', { tab: 'systemView' })).toEqual({ tab: 'systemView' });
    expect(invoke('MemScope.bottomDrawer.setOpen', { open: false })).toEqual({ open: false });
});

test('drawer commands reject after the UI controller unmounts mid-conversation', () => {
    registerDrawer(false, 'sliceDetail');
    const setOpen = handlers.get('MemScope.bottomDrawer.setOpen')!;
    unregisterDrawer?.();
    unregisterDrawer = undefined;

    expect(() => setOpen({ open: true }, {
        requestId: 'request-1',
        deadline: Date.now() + 1000,
        signal: new AbortController().signal,
    })).toThrow(/bottom drawer is unavailable/);
});

test('system table switching is idempotent and resets only the selected destination', () => {
    const session = createSession();
    setMemScopeInteractionSession(session);
    registerDrawer(true, 'systemView');
    session.eventsTableData = [{ id: 1 }] as any;
    session.eventsCurrentPage = 3;

    expect(invoke('MemScope.systemTable.selectView', { view: 'events' })).toEqual({ view: 'events' });
    expect(session.tableType).toBe('events');
    expect(session.eventsTableData).toEqual([]);
    expect(session.eventsCurrentPage).toBe(1);

    session.eventsTableData = [{ id: 2 }] as any;
    expect(invoke('MemScope.systemTable.selectView', { view: 'events' })).toEqual({ view: 'events' });
    expect(session.eventsTableData).toEqual([{ id: 2 }]);
});

test('context commands validate and update the same Session state as page controls', () => {
    const session = createSession();
    setMemScopeInteractionSession(session);

    expect(invoke('MemScope.context.selectDevice', { deviceId: 'device-1' })).toEqual({ deviceId: 'device-1', eventType: 'ACL' });
    expect(session.typeOpts).toEqual([{ label: 'ACL', value: 'ACL' }]);
    expect(() => invoke('MemScope.context.selectEventType', { eventType: 'HOST_PINNED' })).toThrow(/unavailable/);
    expect(() => invoke('MemScope.context.selectThread', { threadId: '202' })).toThrow(/must be an integer/);

    session.searchFunc = ['forward'];
    expect(invoke('MemScope.context.selectThread', { threadId: 101 })).toEqual({ threadId: 101, functions: [] });
    expect(session.searchFunc).toEqual(['forward']);
    expect(invoke('MemScope.context.selectThread', { threadId: 202 })).toEqual({ threadId: 202, functions: [] });
    expect(session.searchFunc).toEqual([]);
    expect(invoke('MemScope.context.setFunctionFilter', { functions: ['backward'] })).toEqual({ functions: ['backward'] });
    expect(() => invoke('MemScope.context.setFunctionFilter', { functions: ['missing'] })).toThrow(/unavailable/);
    expect(() => invoke('MemScope.context.setFunctionFilter', {
        functions: Array.from({ length: 51 }, (_, index) => `function-${index}`),
    })).toThrow(/at most 50/);
});

test('observation reports per-control availability with current and optionCount only', () => {
    const session = createSession();
    setMemScopeInteractionSession(session);
    registerDrawer(true, 'systemView');

    expect(observeMemScopeInteractions()).toEqual({
        bottomDrawer: { available: true, open: true, tab: 'systemView' },
        systemTable: { available: true, view: 'blocks' },
        controls: {
            device: { available: true, current: 'device-0', optionCount: 2 },
            eventType: { available: true, current: 'ACL', optionCount: 2 },
            thread: { available: true, current: 101, optionCount: 2 },
            functions: { available: true, current: ['forward'], selectedCount: 1, optionCount: 3, truncated: false },
        },
    });
});

test('observation collapses unavailable sections instead of reporting stale state', () => {
    registerDrawer(false, 'sliceDetail');

    expect(observeMemScopeInteractions()).toEqual({
        bottomDrawer: { available: true, open: false, tab: 'sliceDetail' },
        systemTable: { available: false, view: null },
        controls: {
            device: { available: false, current: null, optionCount: 0 },
            eventType: { available: false, current: null, optionCount: 0 },
            thread: { available: false, current: null, optionCount: 0 },
            functions: { available: false, current: [], selectedCount: 0, optionCount: 0, truncated: false },
        },
    });
});

test('observation availability mirrors the published command directory', () => {
    const session = createSession();
    setMemScopeInteractionSession(session);

    registerDrawer(true, 'sliceDetail');
    let observation = observeMemScopeInteractions() as { systemTable: { available: boolean } };
    expect(definitions.has('MemScope.systemTable.selectView')).toBe(observation.systemTable.available);

    registerDrawer(true, 'systemView');
    observation = observeMemScopeInteractions() as { systemTable: { available: boolean } };
    expect(definitions.has('MemScope.systemTable.selectView')).toBe(observation.systemTable.available);
    expect(definitions.has('MemScope.systemTable.selectView')).toBe(true);
});

test('getOptions bounds the full function list with query filtering', () => {
    const session = createSession();
    setMemScopeInteractionSession(session);

    expect(invoke('MemScope.context.getOptions', { control: 'functions', query: 'ward', limit: 1 })).toEqual({
        control: 'functions',
        current: ['forward'],
        total: 3,
        matched: 2,
        options: [{ label: 'forward', value: 'forward' }],
        truncated: true,
    });
});
