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
import { runInAction } from 'mobx';
import { COMMAND_ERROR_CODES, CommandError, type JsonObject } from '@insight/lib/FrontendAgentCommand';
import {
    registerDynamicCommands,
    type DynamicCommandBinding,
    type ModuleAgentCommandClient,
} from '@insight/lib/ModuleAgentCommandClient';
import type { Session } from '@/entity/session';

export type BottomDrawerTab = 'sliceDetail' | 'systemView';
export type SystemTableView = 'blocks' | 'events';
type ContextControl = 'device' | 'eventType' | 'thread' | 'functions';
type InteractionCommandId =
    | 'bottomDrawer.setOpen'
    | 'bottomDrawer.selectTab'
    | 'systemTable.selectView'
    | 'context.selectDevice'
    | 'context.selectEventType'
    | 'context.selectThread'
    | 'context.setFunctionFilter';
const MAX_FUNCTION_FILTERS = 50;

interface BottomDrawerController {
    observe: () => { open: boolean; tab: BottomDrawerTab };
    setOpen: (open: boolean) => void;
    selectTab: (tab: BottomDrawerTab) => void;
}

let activeSession: Session | undefined;
let bottomDrawerController: BottomDrawerController | undefined;
const interactionListeners = new Set<() => void>();

export const setMemScopeInteractionSession = (session: Session | undefined): void => {
    activeSession = session;
    notifyInteractionsChanged();
};

export const registerBottomDrawerController = (controller: BottomDrawerController): (() => void) => {
    bottomDrawerController = controller;
    notifyInteractionsChanged();
    return () => {
        if (bottomDrawerController !== controller) return;
        bottomDrawerController = undefined;
        notifyInteractionsChanged();
    };
};

/**
 * Notifies the interaction registry that a control's visibility or option set changed,
 * so dynamically published commands can be synchronized with the real UI.
 */
export const notifyMemScopeInteractionsChanged = (): void => {
    notifyInteractionsChanged();
};

const notifyInteractionsChanged = (): void => {
    interactionListeners.forEach(listener => listener());
};

const subscribeInteractionsChanged = (listener: () => void): (() => void) => {
    interactionListeners.add(listener);
    return () => {
        interactionListeners.delete(listener);
    };
};

const isSystemTableActive = (): boolean => {
    const drawer = bottomDrawerController?.observe();
    return Boolean(activeSession && drawer && drawer.open && drawer.tab === 'systemView');
};

const listInteractionCommandIds = (): InteractionCommandId[] => {
    const session = activeSession;
    const ids: InteractionCommandId[] = [];
    if (bottomDrawerController) {
        ids.push('bottomDrawer.setOpen', 'bottomDrawer.selectTab');
    }
    if (isSystemTableActive()) {
        ids.push('systemTable.selectView');
    }
    if (session) {
        if (session.deviceIdOpts.length > 0) ids.push('context.selectDevice');
        if (session.typeOpts.length > 0) ids.push('context.selectEventType');
        if (session.threadOps.length > 0) ids.push('context.selectThread');
        if (session.funcOptions.length > 0) ids.push('context.setFunctionFilter');
    }
    return ids;
};

export const observeMemScopeInteractions = (): JsonObject => {
    const session = activeSession;
    const drawer = bottomDrawerController?.observe();
    const systemTableActive = Boolean(session && drawer && drawer.open && drawer.tab === 'systemView');
    return {
        bottomDrawer: {
            available: Boolean(drawer),
            open: drawer?.open ?? false,
            tab: drawer?.tab ?? null,
        },
        systemTable: {
            available: systemTableActive,
            view: systemTableActive && session ? session.tableType : null,
        },
        controls: {
            device: controlState(session?.deviceId ?? null, session?.deviceIdOpts.length ?? 0),
            eventType: controlState(session?.eventType ?? null, session?.typeOpts.length ?? 0),
            thread: controlState(session?.threadId ?? null, session?.threadOps.length ?? 0),
            functions: {
                available: Boolean(session && session.funcOptions.length > 0),
                current: session ? session.searchFunc.slice(0, MAX_FUNCTION_FILTERS) : [],
                selectedCount: session?.searchFunc.length ?? 0,
                optionCount: session?.funcOptions.length ?? 0,
                truncated: Boolean(session && session.searchFunc.length > MAX_FUNCTION_FILTERS),
            },
        },
    };
};

const controlState = (current: string | number | null, optionCount: number): JsonObject => ({
    available: optionCount > 0,
    current,
    optionCount,
});

export const registerMemScopeInteractionCommands = (client: ModuleAgentCommandClient): (() => void) => {
    const unregisterResident = client.registerCommand({
        name: 'MemScope.context.getOptions',
        title: 'Get bounded context control options',
        description: 'Returns bounded valid options for a MemScope context control. Function options support query filtering.',
        inputSchema: objectSchema({
            control: { type: 'string', enum: ['device', 'eventType', 'thread', 'functions'] },
            query: { type: 'string' },
            limit: { type: 'integer', minimum: 1, maximum: 100, default: 50 },
        }, ['control']),
    }, (args) => getContextOptions(args));
    const unregisterDynamic = registerDynamicCommands(client, {
        listCommandIds: listInteractionCommandIds,
        subscribeCommandsChanged: subscribeInteractionsChanged,
    }, (commandId) => INTERACTION_COMMANDS[commandId]);
    return () => {
        unregisterDynamic();
        unregisterResident();
    };
};

const INTERACTION_COMMANDS: Record<InteractionCommandId, DynamicCommandBinding> = {
    'bottomDrawer.setOpen': {
        definition: {
            name: 'MemScope.bottomDrawer.setOpen',
            title: 'Open or close the bottom drawer',
            description: 'Sets the MemScope bottom drawer to an explicit open or closed state without toggling it.',
            inputSchema: objectSchema({ open: { type: 'boolean' } }, ['open']),
        },
        handler: (args) => setBottomDrawerOpen(args),
    },
    'bottomDrawer.selectTab': {
        definition: {
            name: 'MemScope.bottomDrawer.selectTab',
            title: 'Select a bottom drawer tab',
            description: 'Selects a tab in the bottom drawer. "sliceDetail" shows detail panels for the currently selected memory event or block (not a table); "systemView" shows the system-level Blocks and Events tables. Open the drawer with MemScope.bottomDrawer.setOpen to inspect the selected content.',
            inputSchema: objectSchema({ tab: { type: 'string', enum: ['sliceDetail', 'systemView'] } }, ['tab']),
        },
        handler: (args) => selectBottomDrawerTab(args),
    },
    'systemTable.selectView': {
        definition: {
            name: 'MemScope.systemTable.selectView',
            title: 'Select the system table view',
            description: 'Switches the table in the System View tab of the bottom drawer between Blocks and Events, applying the same reset behavior as the page control. The drawer must be open on the System View tab.',
            inputSchema: objectSchema({ view: { type: 'string', enum: ['blocks', 'events'] } }, ['view']),
        },
        handler: (args) => selectSystemTable(args),
    },
    'context.selectDevice': {
        definition: {
            name: 'MemScope.context.selectDevice',
            title: 'Select a memory device',
            description: 'Selects a currently available memory device and resets Event Type to that device\'s first option.',
            inputSchema: objectSchema({ deviceId: { type: 'string', minLength: 1 } }, ['deviceId']),
        },
        handler: (args) => selectDevice(args),
    },
    'context.selectEventType': {
        definition: {
            name: 'MemScope.context.selectEventType',
            title: 'Select a memory event type',
            description: 'Selects an event type available for the current memory device.',
            inputSchema: objectSchema({ eventType: { type: 'string', minLength: 1 } }, ['eventType']),
        },
        handler: (args) => selectEventType(args),
    },
    'context.selectThread': {
        definition: {
            name: 'MemScope.context.selectThread',
            title: 'Select a thread',
            description: 'Selects an available thread and clears the current function filter.',
            inputSchema: objectSchema({ threadId: { type: 'integer' } }, ['threadId']),
        },
        handler: (args) => selectThread(args),
    },
    'context.setFunctionFilter': {
        definition: {
            name: 'MemScope.context.setFunctionFilter',
            title: 'Set the function filter',
            description: 'Sets the selected flame-graph functions. Pass an empty array to clear the filter.',
            inputSchema: objectSchema({
                functions: {
                    type: 'array',
                    items: { type: 'string', minLength: 1 },
                    maxItems: MAX_FUNCTION_FILTERS,
                    uniqueItems: true,
                },
            }, ['functions']),
        },
        handler: (args) => setFunctionFilter(args),
    },
};

export const selectSystemTableView = (session: Session, view: SystemTableView): void => {
    if (session.tableType === view) return;
    runInAction(() => {
        session.tableType = view;
        if (view === 'blocks') resetBlocksTable(session);
        else resetEventsTable(session);
    });
};

export const selectMemScopeDevice = (session: Session, deviceId: string): void => {
    const eventTypes = session.deviceIds[deviceId];
    if (!Array.isArray(eventTypes) || eventTypes.length === 0) throw invalid(`Device '${deviceId}' is unavailable.`);
    if (session.deviceId === deviceId) return;
    runInAction(() => {
        session.threadFlag = false;
        session.typeOpts = eventTypes.map((type: string) => ({ label: type, value: type }));
        session.deviceId = deviceId;
        session.eventType = eventTypes[0];
    });
};

export const selectMemScopeEventType = (session: Session, eventType: string): void => {
    const eventTypes = session.deviceIds[session.deviceId];
    if (!Array.isArray(eventTypes) || !eventTypes.includes(eventType)) {
        throw invalid(`Event type '${eventType}' is unavailable for device '${session.deviceId}'.`);
    }
    if (session.eventType === eventType) return;
    runInAction(() => {
        session.threadFlag = false;
        session.eventType = eventType;
    });
};

export const selectMemScopeThread = (session: Session, threadId: number): void => {
    if (!session.threadIds.includes(threadId)) throw invalid(`Thread '${threadId}' is unavailable.`);
    if (session.threadId === threadId) return;
    runInAction(() => {
        session.threadId = threadId;
        session.threadFlag = false;
        session.searchFunc = [];
    });
};

export const setMemScopeFunctionFilter = (session: Session, functions: string[]): void => {
    const valid = new Set(session.funcOptions.map((option: { value: unknown }) => String(option.value)));
    const unavailableFunction = functions.find(name => !valid.has(name));
    if (unavailableFunction) throw invalid(`Function '${unavailableFunction}' is unavailable.`);
    runInAction(() => { session.searchFunc = [...functions]; });
};

const setBottomDrawerOpen = (args: JsonObject): JsonObject => {
    if (typeof args.open !== 'boolean') throw invalid('open must be a boolean.');
    const controller = requireBottomDrawerController();
    controller.setOpen(args.open);
    notifyInteractionsChanged();
    return { open: args.open };
};

const selectBottomDrawerTab = (args: JsonObject): JsonObject => {
    if (args.tab !== 'sliceDetail' && args.tab !== 'systemView') throw invalid('tab must be sliceDetail or systemView.');
    const controller = requireBottomDrawerController();
    controller.selectTab(args.tab);
    notifyInteractionsChanged();
    return { tab: args.tab };
};

const selectSystemTable = (args: JsonObject): JsonObject => {
    if (args.view !== 'blocks' && args.view !== 'events') throw invalid('view must be blocks or events.');
    if (!isSystemTableActive()) throw unavailable('The System View tab must be open and active to select its view.');
    const session = requireSession();
    selectSystemTableView(session, args.view);
    return { view: args.view };
};

const getContextOptions = (args: JsonObject): JsonObject => {
    const session = requireSession();
    const control = contextControlArg(args.control);
    const query = args.query === undefined ? '' : stringValue(args.query, 'query').toLowerCase();
    const limit = integerValue(args.limit, 'limit', 50, 1, 100);
    const options = controlOptions(session, control);
    const matches = query
        ? options.filter(option => `${option.label}\n${String(option.value)}`.toLowerCase().includes(query))
        : options;
    return {
        control,
        current: currentControlValue(session, control),
        total: options.length,
        matched: matches.length,
        options: matches.slice(0, limit),
        truncated: matches.length > limit,
    };
};

const selectDevice = (args: JsonObject): JsonObject => {
    const deviceId = stringValue(args.deviceId, 'deviceId');
    const session = requireSession();
    requireControlAvailable(session.deviceIdOpts.length > 0, 'device');
    selectMemScopeDevice(session, deviceId);
    return { deviceId: session.deviceId, eventType: session.eventType };
};

const selectEventType = (args: JsonObject): JsonObject => {
    const eventType = stringValue(args.eventType, 'eventType');
    const session = requireSession();
    requireControlAvailable(session.typeOpts.length > 0, 'event type');
    selectMemScopeEventType(session, eventType);
    return { deviceId: session.deviceId, eventType: session.eventType };
};

const selectThread = (args: JsonObject): JsonObject => {
    const threadId = integerValue(args.threadId, 'threadId');
    const session = requireSession();
    requireControlAvailable(session.threadOps.length > 0, 'thread');
    selectMemScopeThread(session, threadId);
    return { threadId: session.threadId, functions: [] };
};

const setFunctionFilter = (args: JsonObject): JsonObject => {
    if (!Array.isArray(args.functions) || args.functions.some(name => typeof name !== 'string' || !name.trim())) {
        throw invalid('functions must be an array of non-empty strings.');
    }
    const functions = args.functions.map(name => String(name).trim());
    if (functions.length > MAX_FUNCTION_FILTERS) throw invalid(`functions must contain at most ${MAX_FUNCTION_FILTERS} items.`);
    if (new Set(functions).size !== functions.length) throw invalid('functions must not contain duplicates.');
    const session = requireSession();
    requireControlAvailable(session.funcOptions.length > 0, 'function filter');
    setMemScopeFunctionFilter(session, functions);
    return { functions: [...session.searchFunc] };
};

const resetBlocksTable = (session: Session): void => {
    session.blocksTableData = [];
    session.blocksTableHeader = [];
    session.blocksCurrentPage = 1;
    session.blocksPageSize = 10;
    session.blocksTotal = 0;
    session.blocksOrder = '';
    session.blocksOrderBy = '';
    session.blocksFilters = {};
    session.blocksRangeFilters = {};
};

const resetEventsTable = (session: Session): void => {
    session.eventsTableData = [];
    session.eventsTableHeader = [];
    session.eventsCurrentPage = 1;
    session.eventsPageSize = 10;
    session.eventsTotal = 0;
    session.eventsOrder = '';
    session.eventsOrderBy = '';
    session.eventsFilters = {};
    session.eventsRangeFilters = {};
    session.lazyUsedThreshold = { perT: null, valueT: null };
    session.delayedFreeThreshold = { perT: null, valueT: null };
    session.longIdleThreshold = { perT: null, valueT: null };
    session.onlyInefficient = false;
};

const optionValue = (option: { label: unknown; value: unknown }): JsonObject => ({
    label: String(option.label),
    value: typeof option.value === 'number' ? option.value : String(option.value),
});
const controlOptions = (session: Session, control: ContextControl): JsonObject[] => {
    if (control === 'device') return session.deviceIdOpts.map(optionValue);
    if (control === 'eventType') return session.typeOpts.map(optionValue);
    if (control === 'thread') return session.threadOps.map(optionValue);
    return session.funcOptions.map(optionValue);
};
const currentControlValue = (session: Session, control: ContextControl): string | number | string[] => {
    if (control === 'device') return session.deviceId;
    if (control === 'eventType') return session.eventType;
    if (control === 'thread') return session.threadId;
    return session.searchFunc.slice(0, MAX_FUNCTION_FILTERS);
};
const contextControlArg = (value: unknown): ContextControl => {
    if (value === 'device' || value === 'eventType' || value === 'thread' || value === 'functions') return value;
    throw invalid('control must be device, eventType, thread, or functions.');
};
const requireSession = (): Session => {
    if (!activeSession) throw unavailable('MemScope has no active analysis session.');
    return activeSession;
};
const requireBottomDrawerController = (): BottomDrawerController => {
    if (!bottomDrawerController) throw unavailable('MemScope bottom drawer is unavailable.');
    return bottomDrawerController;
};
const requireControlAvailable = (available: boolean, control: string): void => {
    if (!available) throw unavailable(`The ${control} control is currently unavailable.`);
};
const stringValue = (value: unknown, name: string): string => {
    if (typeof value !== 'string') throw invalid(`${name} must be a string.`);
    return value.trim();
};
const integerValue = (value: unknown, name: string, fallback?: number, minimum?: number, maximum?: number): number => {
    if (value === undefined && fallback !== undefined) return fallback;
    if (typeof value !== 'number' ||
        !Number.isInteger(value) ||
        (minimum !== undefined && value < minimum) ||
        (maximum !== undefined && value > maximum)) {
        throw invalid(`${name} must be an integer${minimum === undefined ? '' : ` between ${minimum} and ${maximum}`}.`);
    }
    return value;
};
function objectSchema(properties: JsonObject, required: string[]): JsonObject {
    return { type: 'object', properties, required, additionalProperties: false };
}
const invalid = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.INVALID, message, retryable: false,
});
const unavailable = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.UNAVAILABLE, message, retryable: true,
});
