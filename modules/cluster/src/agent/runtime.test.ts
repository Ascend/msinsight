/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import { act, cleanup, render } from '@testing-library/react';
import { runInAction } from 'mobx';
import {
    COMMAND_ERROR_CODES, MODULE_AGENT_COMMAND_ERROR, MODULE_AGENT_COMMAND_RESPONSE,
    MODULE_AGENT_COMMANDS_CHANGED, MODULE_AGENT_EXECUTE_COMMAND, MODULE_AGENT_HELLO,
    MODULE_AGENT_MESSAGE_CHANNEL, MODULE_AGENT_OBSERVE, MODULE_AGENT_READY,
} from '@insight/lib/FrontendAgentCommand';
import { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { Session } from '../entity/session';
import { RootStoreContext } from '../context/context';
import { RootStore } from '../store/rootStore';
import { observeCommunication, setCommunicationAgentSession } from './communicationController';
import { startCommunicationAgentRuntime, stopCommunicationAgentRuntime } from './runtime';

const mockBootstrapRender = jest.fn();

jest.mock('@insight/lib', () => ({
    SharedConfigProvider: ({ children }: { children: React.ReactNode }) => children,
}), { virtual: true });
jest.mock('@insight/lib/theme', () => ({
    GlobalStyles: () => null,
    themeInstance: { setCurrentTheme: jest.fn(), getThemeType: () => ({}) },
}), { virtual: true });
jest.mock('../theme/theme', () => ({ themeInstance: { setCurrentTheme: jest.fn(), getThemeType: () => ({}) } }));
jest.mock('../connection', () => ({ __esModule: true, default: { send: jest.fn() } }));
jest.mock('../index', () => ({ Loading: null }));
jest.mock('../components/communication/CommunicationAnalysis', () => ({ __esModule: true, default: () => null }));
jest.mock('../pages/AnalysisSummary', () => ({ __esModule: true, default: () => null }));
jest.mock('react-dom/client', () => {
    const actual = jest.requireActual<typeof import('react-dom/client')>('react-dom/client');
    return {
        ...actual,
        createRoot: (container: Parameters<typeof actual.createRoot>[0], options?: Parameters<typeof actual.createRoot>[1]) => {
            // Suppress entry bootstrap only; Testing Library mounts real React roots.
            if (container instanceof globalThis.HTMLElement && container.id === 'root') {
                return { render: mockBootstrapRender, unmount: jest.fn() };
            }
            return actual.createRoot(container, options);
        },
    };
});

let CommunicationApp: typeof import('../CommunicationIndex')['App'];
let SummaryApp: typeof import('../SummaryIndex')['App'];
let rootElement: HTMLDivElement;

beforeAll(() => {
    rootElement = document.createElement('div');
    rootElement.id = 'root';
    document.body.appendChild(rootElement);
    CommunicationApp = jest.requireActual<typeof import('../CommunicationIndex')>('../CommunicationIndex').App;
    SummaryApp = jest.requireActual<typeof import('../SummaryIndex')>('../SummaryIndex').App;
});
afterAll(() => rootElement.remove());

let parentDescriptor: PropertyDescriptor | undefined;
let parent: Window;
let post: jest.Mock;

beforeEach(() => {
    window.setTheme = jest.fn();
    parentDescriptor = Object.getOwnPropertyDescriptor(window, 'parent');
    post = jest.fn();
    parent = { postMessage: post } as unknown as Window;
    Object.defineProperty(window, 'parent', { configurable: true, value: parent });
});
afterEach(() => {
    cleanup();
    stopCommunicationAgentRuntime();
    setCommunicationAgentSession(undefined);
    jest.restoreAllMocks();
    if (parentDescriptor) Object.defineProperty(window, 'parent', parentDescriptor);
    else Reflect.deleteProperty(window, 'parent');
});

const send = (event: string, connectionToken = 'connection-1', extra: object = {}): void => {
    window.dispatchEvent(new MessageEvent('message', {
        data: { channel: MODULE_AGENT_MESSAGE_CHANNEL, event, moduleId: 'Communication', connectionToken, ...extra },
        source: parent,
        origin: window.location.origin,
    }));
};
const flush = async (): Promise<void> => {
    await Promise.resolve();
    await Promise.resolve();
    await Promise.resolve();
};
const observe = async (requestId: string): Promise<void> => {
    post.mockClear();
    send(MODULE_AGENT_OBSERVE, 'connection-1', { requestId, deadline: Date.now() + 5000 });
    await flush();
};

describe('Communication agent runtime with the actual module client', () => {
    test('starts one client and advertises an empty command catalog', async () => {
        const start = jest.spyOn(ModuleAgentCommandClient.prototype, 'start');
        startCommunicationAgentRuntime();
        startCommunicationAgentRuntime();
        expect(start).toHaveBeenCalledTimes(1);
        send(MODULE_AGENT_HELLO);
        await flush();
        expect(post).toHaveBeenCalledTimes(2);
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            event: MODULE_AGENT_READY, moduleId: 'Communication', connectionToken: 'connection-1',
        }), window.location.origin);
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            event: MODULE_AGENT_COMMANDS_CHANGED, commands: [], connectionToken: 'connection-1',
        }), window.location.origin);
    });

    test('stop is idempotent and restart reuses the same client', async () => {
        const start = jest.spyOn(ModuleAgentCommandClient.prototype, 'start');
        const dispose = jest.spyOn(ModuleAgentCommandClient.prototype, 'dispose');
        startCommunicationAgentRuntime();
        send(MODULE_AGENT_HELLO);
        await flush();
        stopCommunicationAgentRuntime();
        stopCommunicationAgentRuntime();
        expect(dispose).toHaveBeenCalledTimes(1);
        post.mockClear();
        send(MODULE_AGENT_HELLO, 'ignored');
        await flush();
        expect(post).not.toHaveBeenCalled();
        startCommunicationAgentRuntime();
        expect(start).toHaveBeenCalledTimes(2);
        expect(start.mock.instances[0]).toBe(start.mock.instances[1]);
        send(MODULE_AGENT_HELLO, 'connection-2');
        await flush();
        expect(post).toHaveBeenCalledTimes(2);
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            event: MODULE_AGENT_COMMANDS_CHANGED, commands: [], connectionToken: 'connection-2',
        }), window.location.origin);
    });

    test('stopping before startup does not prevent a later handshake', async () => {
        stopCommunicationAgentRuntime();
        send(MODULE_AGENT_HELLO);
        await flush();
        expect(post).not.toHaveBeenCalled();
        startCommunicationAgentRuntime();
        send(MODULE_AGENT_HELLO);
        await flush();
        expect(post).toHaveBeenCalledTimes(2);
    });

    test('observes the controller session and reads replacements without restarting', async () => {
        const session = new Session();
        runInAction(() => { session.unitcount = 1; });
        setCommunicationAgentSession(session);
        startCommunicationAgentRuntime();
        send(MODULE_AGENT_HELLO);
        await flush();
        await observe('observe-1');
        expect(post).toHaveBeenCalledTimes(1);
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            event: MODULE_AGENT_COMMAND_RESPONSE,
            requestId: 'observe-1',
            result: expect.objectContaining({
                moduleId: 'Communication',
                available: true,
                analysisType: null,
                scope: null,
                selection: null,
                dataStatus: expect.objectContaining({ hasTimelineUnits: true }),
            }),
        }), window.location.origin);
        setCommunicationAgentSession(new Session());
        await observe('observe-2');
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            requestId: 'observe-2',
            result: expect.objectContaining({ available: true, dataStatus: expect.objectContaining({ hasTimelineUnits: false }) }),
        }), window.location.origin);
    });

    test('reports unavailable after the controller session is cleared', async () => {
        setCommunicationAgentSession(new Session());
        startCommunicationAgentRuntime();
        send(MODULE_AGENT_HELLO);
        await flush();
        setCommunicationAgentSession(undefined);
        await observe('observe-cleared');
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            event: MODULE_AGENT_COMMAND_RESPONSE,
            requestId: 'observe-cleared',
            result: expect.objectContaining({ available: false, dataStatus: null, analysisType: null, scope: null, selection: null }),
        }), window.location.origin);
    });

    test('does not advertise or execute a placeholder business command', async () => {
        startCommunicationAgentRuntime();
        send(MODULE_AGENT_HELLO);
        await flush();
        post.mockClear();
        send(MODULE_AGENT_EXECUTE_COMMAND, 'connection-1', {
            requestId: 'unsupported', command: 'Communication.test.notImplemented', args: {}, deadline: Date.now() + 5000,
        });
        await flush();
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            event: MODULE_AGENT_COMMAND_ERROR,
            requestId: 'unsupported',
            error: expect.objectContaining({ code: COMMAND_ERROR_CODES.UNAVAILABLE }),
        }), window.location.origin);
    });
});

const wrap = (store: RootStore, child: React.ReactNode): React.ReactElement => (
    React.createElement(RootStoreContext.Provider, { value: store }, child)
);

describe('Communication entry with the actual runtime', () => {
    test('mount starts the real runtime and unmount clears the session and listener', async () => {
        const start = jest.spyOn(ModuleAgentCommandClient.prototype, 'start');
        const dispose = jest.spyOn(ModuleAgentCommandClient.prototype, 'dispose');
        const store = new RootStore();
        const view = render(wrap(store, React.createElement(CommunicationApp)));
        expect(start).toHaveBeenCalledTimes(1);
        send(MODULE_AGENT_HELLO);
        await flush();
        await observe('mounted');
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            requestId: 'mounted', result: expect.objectContaining({ available: true }),
        }), window.location.origin);
        view.unmount();
        expect(dispose).toHaveBeenCalledTimes(1);
        expect(observeCommunication().available).toBe(false);
        post.mockClear();
        send(MODULE_AGENT_HELLO, 'after-unmount');
        await flush();
        expect(post).not.toHaveBeenCalled();
    });

    test('session replacement reaches observe without restarting the client', async () => {
        const start = jest.spyOn(ModuleAgentCommandClient.prototype, 'start');
        const dispose = jest.spyOn(ModuleAgentCommandClient.prototype, 'dispose');
        const store = new RootStore();
        runInAction(() => { store.sessionStore.activeSession.unitcount = 1; });
        render(wrap(store, React.createElement(CommunicationApp)));
        send(MODULE_AGENT_HELLO);
        await flush();
        await observe('before-replacement');
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            requestId: 'before-replacement',
            result: expect.objectContaining({ available: true, dataStatus: expect.objectContaining({ hasTimelineUnits: true }) }),
        }), window.location.origin);
        act(() => { runInAction(() => { store.sessionStore.activeSession = new Session(); }); });
        await observe('after-replacement');
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            requestId: 'after-replacement',
            result: expect.objectContaining({ available: true, dataStatus: expect.objectContaining({ hasTimelineUnits: false }) }),
        }), window.location.origin);
        expect(start).toHaveBeenCalledTimes(1);
        expect(dispose).not.toHaveBeenCalled();
    });

    test('ordinary rerenders do not restart the client', () => {
        const start = jest.spyOn(ModuleAgentCommandClient.prototype, 'start');
        const dispose = jest.spyOn(ModuleAgentCommandClient.prototype, 'dispose');
        const store = new RootStore();
        const view = render(wrap(store, React.createElement(CommunicationApp)));
        act(() => { runInAction(() => { store.sessionStore.activeSession.language = 'zhCN'; }); });
        view.rerender(wrap(store, React.createElement(CommunicationApp)));
        expect(start).toHaveBeenCalledTimes(1);
        expect(dispose).not.toHaveBeenCalled();
        expect(observeCommunication().available).toBe(true);
    });

    test('StrictMode replay leaves one live client and one response per request', async () => {
        const start = jest.spyOn(ModuleAgentCommandClient.prototype, 'start');
        const dispose = jest.spyOn(ModuleAgentCommandClient.prototype, 'dispose');
        const store = new RootStore();
        const view = render(wrap(store, React.createElement(React.StrictMode, null, React.createElement(CommunicationApp))));
        expect(start).toHaveBeenCalledTimes(2);
        expect(start.mock.instances[0]).toBe(start.mock.instances[1]);
        expect(dispose).toHaveBeenCalledTimes(1);
        send(MODULE_AGENT_HELLO);
        await flush();
        expect(post).toHaveBeenCalledTimes(2);
        await observe('strict-mode');
        expect(post).toHaveBeenCalledTimes(1);
        expect(post).toHaveBeenCalledWith(expect.objectContaining({
            requestId: 'strict-mode', result: expect.objectContaining({ available: true }),
        }), window.location.origin);
        view.unmount();
        expect(dispose).toHaveBeenCalledTimes(2);
        expect(observeCommunication().available).toBe(false);
    });

    test('Summary does not start the Communication runtime or bind its session', async () => {
        const start = jest.spyOn(ModuleAgentCommandClient.prototype, 'start');
        const view = render(wrap(new RootStore(), React.createElement(SummaryApp)));
        expect(start).not.toHaveBeenCalled();
        expect(observeCommunication().available).toBe(false);
        send(MODULE_AGENT_HELLO);
        await flush();
        expect(post).not.toHaveBeenCalled();
        view.unmount();
    });
});
