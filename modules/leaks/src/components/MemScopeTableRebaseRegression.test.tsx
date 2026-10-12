/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import '@testing-library/jest-dom';
import { act, cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
import { runInAction } from 'mobx';
import { TABLE_ERROR_CODES, type TableController, type TableCommandId, type TransitionContext } from '@insight/lib/AgentTable';
import { Session } from '../entity/session';
import { memScopeTableControllerRegistry } from '../agent/runtime';
import { createMemoryBlockContextKey } from './blockLoadState';
import { getBlockTableData, getEventTableData } from './dataHandler';
import BlocksTable from './BlocksTable';
import EventsTable from './EventsTable';

type Kind = 'blocks' | 'events';
const mockTableProps = new Map<string, any>();
const mockControllers: TableController[] = [];

jest.mock('./dataHandler', () => ({ getBlockTableData: jest.fn(), getEventTableData: jest.fn() }));
jest.mock('../agent/runtime', () => {
    const { TableControllerRegistry } = require('@insight/lib/AgentTable');
    return { memScopeTableControllerRegistry: new TableControllerRegistry() };
});
jest.mock('react-i18next', () => {
    const t = (key: string) => key;
    return { useTranslation: () => ({ t }) };
});
jest.mock('@insight/lib/i18n', () => ({ __esModule: true, default: { t: (key: string) => key } }));
// The utils barrel eagerly loads general-purpose UI; table business helpers remain real.
jest.mock('@insight/lib/utils', () => ({
    safeJSONParse: (text: string) => {
        try { return JSON.parse(text); } catch { return null; }
    },
}));
jest.mock('antd', () => {
    const ReactModule = require('react');
    return { Tag: ({ children }: any) => ReactModule.createElement('span', null, children) };
});
jest.mock('@insight/lib/components', () => {
    const ReactModule = require('react');
    return { Tooltip: ({ children }: any) => ReactModule.createElement('span', null, children) };
});
jest.mock('@insight/lib/resize', () => {
    const ReactModule = require('react');
    return {
        fetchColumnFilterProps: () => ({}),
        ResizeTable: ReactModule.forwardRef((props: any, ref: any) => {
            mockTableProps.set(props['data-testid'], props);
            ReactModule.useImperativeHandle(ref, () => ({ copy: async () => undefined }));
            return ReactModule.createElement('div', {
                'data-testid': props['data-testid'], 'data-loading': String(props.loading),
            }, props.dataSource.map((row: any, index: number) => ReactModule.createElement('div', { key: index },
                props.columns.map((column: any) => ReactModule.createElement('span', { key: column.key },
                    column.render ? column.render(row[column.dataIndex], row) : String(row[column.dataIndex] ?? ''),
                )),
            )));
        }),
    };
});

const headers = [
    { key: 'ID', name: 'ID', sortable: true, searchable: false },
    { key: 'size', name: 'Size', sortable: true, searchable: false, rangeFilterable: true },
    { key: 'Alloc Event ID', name: 'Alloc Event ID', sortable: false, searchable: false },
    { key: 'Free Event ID', name: 'Free Event ID', sortable: false, searchable: false },
];
const result = (kind: Kind, id = 1, total = 50): any => ({
    headers,
    [kind]: [{ ID: id, size: 1024, 'Alloc Event ID': 100, 'Free Event ID': -1 }],
    total,
});
const handler = (kind: Kind): jest.Mock => (kind === 'blocks' ? getBlockTableData : getEventTableData) as jest.Mock;
const rows = (session: Session, kind: Kind): any[] => (session as any)[`${kind}TableData`];
const page = (session: Session, kind: Kind): number => (session as any)[`${kind}CurrentPage`];
const setPage = (session: Session, kind: Kind, value: number): void => { (session as any)[`${kind}CurrentPage`] = value; };
const createSession = (kind: Kind): Session => {
    const session = new Session();
    runInAction(() => {
        session.module = 'memsnapshot';
        session.fileHash = 'project-a';
        session.deviceId = '0';
        session.deviceIds = { 0: ['BLOCK'] };
        session.eventType = 'BLOCK';
        session.tableType = kind;
        session.minTime = 10;
        session.maxTime = 100;
        session.selectedSliceIndex = 0;
        session.snapshotSlices = {
            0: {
                eventCount: 300, sliceCount: 3, readySlices: [0, 1],
                slices: [
                    { index: 0, startEventId: 0, endEventId: 99, ready: true },
                    { index: 1, startEventId: 100, endEventId: 199, ready: true },
                    { index: 2, startEventId: 200, endEventId: 299, ready: false },
                ],
            },
        };
        session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
    });
    return session;
};
const deferred = (): { promise: Promise<any>; resolve: (value: any) => void; reject: (reason: Error) => void } => {
    let resolve!: (value: any) => void;
    let reject!: (reason: Error) => void;
    const promise = new Promise((accept, fail) => { resolve = accept; reject = fail; });
    return { promise, resolve, reject };
};
const commandContext = (requestId = 'test-request'): TransitionContext => ({
    requestId, transitionId: requestId, source: 'agent', deadline: Date.now() + 30000, signal: new AbortController().signal,
});
const execute = (controller: TableController, commandId: TableCommandId, args = {}, context = commandContext()) => controller.execute({
    targetId: 'component-test', expectedRevision: 1, commandId, args, requestId: context.requestId, deadline: context.deadline,
}, context);
const mount = async (kind: Kind, session = createSession(kind)) => {
    const Component = kind === 'blocks' ? BlocksTable : EventsTable;
    const rendered = render(<Component session={session} visible />);
    await waitFor(() => expect(rows(session, kind)).toHaveLength(1));
    await waitFor(() => expect(mockControllers[mockControllers.length - 1].getAvailability().busy).toBe(false));
    return { ...rendered, session, controller: mockControllers[mockControllers.length - 1] };
};

beforeAll(() => {
    let id = 0;
    Object.defineProperty(globalThis, 'crypto', {
        configurable: true, value: { randomUUID: () => `table-test-${++id}` },
    });
});
beforeEach(() => {
    mockTableProps.clear();
    mockControllers.length = 0;
    handler('blocks').mockReset().mockResolvedValue(result('blocks'));
    handler('events').mockReset().mockResolvedValue(result('events'));
    const register = memScopeTableControllerRegistry.register.bind(memScopeTableControllerRegistry);
    jest.spyOn(memScopeTableControllerRegistry, 'register').mockImplementation((controller) => {
        mockControllers.push(controller);
        return register(controller);
    });
});
afterEach(() => {
    cleanup();
    memScopeTableControllerRegistry.dispose();
    jest.restoreAllMocks();
});

describe.each<Kind>(['blocks', 'events'])('stop006 real %s component', kind => {
    test('rejects undefined before reading total and preserves the stable table', async () => {
        const { session, controller } = await mount(kind);
        handler(kind).mockResolvedValueOnce(undefined);
        await act(async () => {
            await expect(execute(controller, 'table.refresh')).rejects.toMatchObject({
                code: TABLE_ERROR_CODES.COMMAND_SUPERSEDED, retryable: true,
            });
        });
        expect(rows(session, kind)[0].ID).toBe(1);
        expect(controller.getSnapshot().state.query.page).toBe(1);
        expect(controller.getAvailability().busy).toBe(false);
    });

    test.each(['headers', 'rows', 'total'])('rejects malformed %s without publishing it', async badField => {
        const { session, controller } = await mount(kind);
        const malformed = result(kind, 9);
        if (badField === 'headers') malformed.headers = [{ key: 'ID' }];
        if (badField === 'rows') malformed[kind] = null;
        if (badField === 'total') malformed.total = NaN;
        handler(kind).mockResolvedValueOnce(malformed);
        await act(async () => {
            await expect(execute(controller, 'table.refresh')).rejects.toMatchObject({
                code: TABLE_ERROR_CODES.TRANSITION_FAILED,
            });
        });
        expect(rows(session, kind)[0].ID).toBe(1);
        expect(controller.getAvailability().busy).toBe(false);
    });

    test('accepts a valid empty response and publishes total zero', async () => {
        const { session, controller } = await mount(kind);
        handler(kind).mockResolvedValueOnce({ headers, [kind]: [], total: 0 });
        await act(async () => { await execute(controller, 'table.refresh'); });
        expect(rows(session, kind)).toEqual([]);
        expect(controller.getSnapshot().state).toMatchObject({ total: 0, rowCount: 0, query: { page: 1 } });
    });

    test.each([false, true])('checks the second clamp response (undefined=%s)', async unavailable => {
        const { session, controller } = await mount(kind);
        handler(kind).mockClear().mockResolvedValueOnce(result(kind, 7, 12))
            .mockResolvedValueOnce(unavailable ? undefined : result(kind, 8, 12));
        await act(async () => {
            const transition = execute(controller, 'table.goToPage', { page: 3 });
            if (unavailable) await expect(transition).rejects.toMatchObject({ code: TABLE_ERROR_CODES.COMMAND_SUPERSEDED });
            else await transition;
        });
        expect(handler(kind)).toHaveBeenCalledTimes(2);
        expect(page(session, kind)).toBe(unavailable ? 1 : 2);
        expect(rows(session, kind)[0].ID).toBe(unavailable ? 1 : 8);
    });

    test('refreshes identical-range slices and rejects the old context revision', async () => {
        const { session } = await mount(kind);
        const before = memScopeTableControllerRegistry.observe()[0];
        handler(kind).mockClear();
        act(() => runInAction(() => {
            session.selectedSliceIndex = 1;
            session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
        }));
        await waitFor(() => expect(handler(kind)).toHaveBeenCalledTimes(1));
        await waitFor(() => expect(memScopeTableControllerRegistry.observe()[0].availability.busy).toBe(false));
        const after = memScopeTableControllerRegistry.observe()[0];
        expect(after.targetId).toBe(before.targetId);
        expect(after.state.contextKey).not.toBe(before.state.contextKey);
        expect(after.revision).toBeGreaterThan(before.revision);
        await expect(memScopeTableControllerRegistry.invoke({
            targetId: before.targetId, expectedRevision: before.revision, commandId: 'table.refresh', args: {},
            requestId: 'old-observation', deadline: Date.now() + 1000,
        })).rejects.toMatchObject({ code: TABLE_ERROR_CODES.STATE_STALE });
    });

    test.each(['project', 'identity', 'restore'])('does not roll back a new %s after a late rejection', async change => {
        const { session, controller } = await mount(kind);
        const old = deferred();
        handler(kind).mockReturnValueOnce(old.promise);
        let transition!: Promise<any>;
        act(() => { transition = execute(controller, 'table.goToPage', { page: 3 }); });
        const observedError = transition.catch(error => error);
        act(() => runInAction(() => {
            if (change === 'project') session.fileHash = 'project-b';
            if (change === 'identity') session.deviceIds = { 0: ['BLOCK'] };
            if (change === 'restore') session.tableKey++;
            setPage(session, kind, 2);
            session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
        }));
        await waitFor(() => expect(controller.getSnapshot().state.query.page).toBe(2));
        await act(async () => { old.reject(new Error('old context failed')); await observedError; });
        expect(page(session, kind)).toBe(2);
        expect(controller.getSnapshot().state.query.page).toBe(2);
    });

    test('cancel invalidates a late success without undoing a newer user page', async () => {
        const { session, controller } = await mount(kind);
        const old = deferred();
        handler(kind).mockReturnValueOnce(old.promise);
        let transition!: Promise<any>;
        act(() => { transition = execute(controller, 'table.goToPage', { page: 3 }, commandContext('cancel-me')); });
        const observedError = transition.catch(error => error);
        act(() => controller.cancel?.('cancel-me'));
        expect(page(session, kind)).toBe(1);
        await act(async () => { await execute(controller, 'table.goToPage', { page: 2 }, commandContext('new-page')); });
        await act(async () => { old.resolve(result(kind, 99)); });
        expect((await observedError).code).toBe(TABLE_ERROR_CODES.COMMAND_SUPERSEDED);
        expect(page(session, kind)).toBe(2);
        expect(rows(session, kind)[0].ID).toBe(1);
    });

    test.each([true, false])('checks an aborted signal before accepting a response (reason=%s)', async withReason => {
        const { session, controller } = await mount(kind);
        const old = deferred();
        const abort = new AbortController();
        const callerReason = new Error('cancelled by caller');
        handler(kind).mockReturnValueOnce(old.promise);
        let transition!: Promise<any>;
        act(() => {
            transition = execute(controller, 'table.goToPage', { page: 3 }, {
                ...commandContext('abort-me'), signal: abort.signal,
            });
        });
        const observedError = transition.catch(error => error);
        abort.abort();
        // Older jsdom signals lack reason; explicitly exercise both runtime contracts.
        Object.defineProperty(abort.signal, 'reason', {
            configurable: true, value: withReason ? callerReason : undefined,
        });
        await act(async () => { old.resolve(result(kind, 99)); await observedError; });
        if (withReason) expect(await observedError).toBe(callerReason);
        else expect(await observedError).toMatchObject({ code: TABLE_ERROR_CODES.COMMAND_CANCELLED, retryable: true });
        expect(page(session, kind)).toBe(1);
        expect(controller.getSnapshot().state.query.page).toBe(1);
        expect(rows(session, kind)[0].ID).toBe(1);
        expect(controller.getAvailability().busy).toBe(false);
    });

    test('unmount invalidates a pending user request', async () => {
        const { session, unmount } = await mount(kind);
        const old = deferred();
        handler(kind).mockReturnValueOnce(old.promise);
        act(() => mockTableProps.get(`${kind}Table`).pagination.onChange(3, 10));
        unmount();
        runInAction(() => setPage(session, kind, 2));
        await act(async () => { old.resolve(result(kind, 99)); await Promise.resolve(); });
        expect(page(session, kind)).toBe(2);
        expect(rows(session, kind)[0].ID).toBe(1);
    });

    test('reloads a same-window table cleared by graph finalization', async () => {
        const { session, controller } = await mount(kind);
        handler(kind).mockClear();
        act(() => runInAction(() => {
            (session as any)[`${kind}TableData`] = [];
            (session as any)[`${kind}Total`] = 0;
            session.loadedMemoryBlockContextKey = '';
            session.loadingBlocks = true;
        }));
        if (kind === 'events') expect(handler(kind)).not.toHaveBeenCalled();
        else await waitFor(() => expect(handler(kind)).toHaveBeenCalledTimes(1));
        act(() => runInAction(() => {
            session.loadingBlocks = false;
            session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
        }));
        await waitFor(() => expect(handler(kind)).toHaveBeenCalledTimes(kind === 'blocks' ? 2 : 1));
        await waitFor(() => expect(rows(session, kind)).toHaveLength(1));
        await waitFor(() => expect(controller.getAvailability().busy).toBe(false));
    });

    test('does not query an unready slice, then resumes when it becomes ready', async () => {
        const session = createSession(kind);
        runInAction(() => {
            session.selectedSliceIndex = 2;
            session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
        });
        const Component = kind === 'blocks' ? BlocksTable : EventsTable;
        render(<Component session={session} visible />);
        expect(handler(kind)).not.toHaveBeenCalled();
        const controller = mockControllers[0];
        await expect(execute(controller, 'table.refresh')).rejects.toMatchObject({ code: TABLE_ERROR_CODES.NOT_READY });
        act(() => runInAction(() => {
            session.snapshotSlices['0'].slices[2].ready = true;
            session.snapshotSlices['0'].readySlices.push(2);
        }));
        await waitFor(() => expect(handler(kind)).toHaveBeenCalledTimes(1));
        await waitFor(() => expect(rows(session, kind)).toHaveLength(1));
    });
});

test('Blocks can query when graph loading fails; event links still locate across ready slices', async () => {
    const session = createSession('blocks');
    runInAction(() => { session.loadedMemoryBlockContextKey = ''; });
    const { controller } = await mount('blocks', session);
    expect(handler('blocks')).toHaveBeenCalledTimes(1);
    expect(screen.getByRole('button', { name: '-1' })).toBeDisabled();
    await act(async () => {
        fireEvent.click(screen.getByRole('button', { name: '100' }));
        await Promise.resolve();
    });
    await waitFor(() => expect(handler('blocks')).toHaveBeenCalledTimes(2));
    await waitFor(() => expect(controller.getAvailability().busy).toBe(false));
    expect(rows(session, 'blocks')).toHaveLength(1);
    expect(session.selectedSliceIndex).toBe(1);
    expect(session.pendingEventLocate).toEqual({ eventId: 100, deviceId: '0' });
});

test('Events wait for the selected graph even when the slice itself is ready', async () => {
    const session = createSession('events');
    runInAction(() => { session.loadedMemoryBlockContextKey = ''; });
    render(<EventsTable session={session} visible />);
    expect(handler('events')).not.toHaveBeenCalled();
    expect(mockControllers[0].getAvailability().ready).toBe(false);
    act(() => runInAction(() => { session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session); }));
    await waitFor(() => expect(handler('events')).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(rows(session, 'events')).toHaveLength(1));
});
