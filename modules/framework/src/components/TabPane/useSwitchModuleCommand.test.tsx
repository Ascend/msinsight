/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React, { useState } from 'react';
import { act, fireEvent, render, screen } from '@testing-library/react';
import { COMMAND_ERROR_CODES, type CommandContext, type CommandHandler, type JsonObject } from '@insight/lib/FrontendAgentCommand';
import { FrontendAgentCommandController } from '../../agent/frontendAgentCommandController/FrontendAgentCommandController';
import { useSwitchModuleCommand } from './useSwitchModuleCommand';
import { getSelectableModules, modulesConfig, type ModuleConfig } from '../../moduleConfig';

const context = (signal = new AbortController().signal): CommandContext => ({ requestId: 'switch', deadline: Date.now() + 10000, signal });

function setup(modules = ['Timeline', 'Memory', 'Plugin']) {
    const controller = new FrontendAgentCommandController();
    const register = jest.spyOn(controller, 'registerGlobalCommand');
    const routing = jest.spyOn(controller, 'setActiveModule');
    function Harness({ choices }: { choices: string[] }): JSX.Element {
        const [active, setActive] = useState('Timeline');
        useSwitchModuleCommand(controller, active, choices, setActive);
        return <div><div data-testid="active">{active}</div><button onClick={() => setActive('Plugin')}>UI switch</button></div>;
    }
    const view = render(<Harness choices={modules} />);
    const handler: CommandHandler = register.mock.calls[0][1];
    return { controller, routing, handler, view, rerender: (choices: string[]) => view.rerender(<Harness choices={choices} />) };
}

describe('framework.switchModule React/controller integration', () => {
    test('menu filtering preserves plugins and special-mode precedence', () => {
        const plugin: ModuleConfig = { name: 'Plugin', requestName: 'plugin', attributes: {}, isDefault: true };
        const normal = [...modulesConfig.filter(module => module.isDefault), plugin];
        expect(getSelectableModules(normal).map(module => module.name)).toContain('Plugin');
        const leaks = modulesConfig.find(module => module.name === 'MemScope') as ModuleConfig;
        const triton = modulesConfig.find(module => module.name === 'On-Chip Memory') as ModuleConfig;
        expect(getSelectableModules([...normal, leaks]).map(module => module.name)).toEqual(['MemScope']);
        expect(getSelectableModules([...normal, triton]).map(module => module.name)).toEqual(['On-Chip Memory']);
        expect(getSelectableModules([...normal, leaks, triton]).map(module => module.name)).toEqual(['MemScope']);
    });
    test.each([{}, { moduleId: '' }, { moduleId: '  ' }, { moduleId: 42 }, { moduleId: null }, { moduleId: 'Memory', extra: true }])(
        'rejects invalid args %j without navigating', args => {
            const { handler, view } = setup();
            expect(() => handler(args as JsonObject, context())).toThrow(expect.objectContaining({ code: COMMAND_ERROR_CODES.INVALID }));
            expect(screen.getByTestId('active').textContent).toBe('Timeline');
            view.unmount();
        },
    );

    test('uses the latest menu choices, permits plugins without commands, and is discoverable', async () => {
        const { controller, handler, view, rerender } = setup();
        const help = await controller.replayCommandForDebug('help', { command: 'framework.switchModule' });
        expect(help).toMatchObject({ command: { approval: 'none', inputSchema: { additionalProperties: false, required: ['moduleId'] } } });
        let result: unknown;
        await act(async () => { result = controller.replayCommandForDebug('framework.switchModule', { moduleId: ' Plugin ' }); });
        expect(await result).toEqual({ activeModule: 'Plugin' });
        rerender(['MemScope']);
        expect(() => handler({ moduleId: 'Memory' }, context())).toThrow(expect.objectContaining({
            code: COMMAND_ERROR_CODES.UNAVAILABLE, details: { availableModules: ['MemScope'] },
        }));
        rerender(['On-Chip Memory']);
        expect(() => handler({ moduleId: 'Plugin' }, context())).toThrow(expect.objectContaining({
            code: COMMAND_ERROR_CODES.UNAVAILABLE, details: { availableModules: ['On-Chip Memory'] },
        }));
        view.unmount();
        await expect(controller.replayCommandForDebug('help', { command: 'framework.switchModule' })).rejects.toMatchObject({ code: COMMAND_ERROR_CODES.UNAVAILABLE });
        controller.dispose();
    });

    test('resolves only after React commit and controller update, including idempotent switches', async () => {
        const { handler, routing, view } = setup();
        for (const moduleId of ['Memory', 'Memory']) {
            let promise: Promise<unknown> = Promise.resolve();
            let settled = false;
            act(() => {
                promise = Promise.resolve(handler({ moduleId }, context()));
                void promise.then(() => {
                    settled = true;
                    expect(screen.getByTestId('active').textContent).toBe(moduleId);
                    expect(routing).toHaveBeenLastCalledWith(moduleId);
                });
                expect(settled).toBe(false);
            });
            await expect(promise).resolves.toEqual({ activeModule: moduleId });
            expect(settled).toBe(true);
        }
        view.unmount();
    });

    test('rejects overlapping requests rather than acknowledging a batched-away target', async () => {
        const { handler, view } = setup();
        let first: unknown;
        act(() => {
            first = handler({ moduleId: 'Memory' }, context());
            expect(() => handler({ moduleId: 'Plugin' }, context())).toThrow(expect.objectContaining({ code: COMMAND_ERROR_CODES.BUSY }));
        });
        await expect(first).resolves.toEqual({ activeModule: 'Memory' });
        expect(screen.getByTestId('active').textContent).toBe('Memory');
        view.unmount();
    });

    test('an external UI switch does not falsely acknowledge the command target', async () => {
        const { handler, view } = setup();
        let outcome: Promise<unknown> = Promise.resolve();
        act(() => {
            outcome = Promise.resolve(handler({ moduleId: 'Memory' }, context())).catch(error => error);
            fireEvent.click(screen.getByText('UI switch'));
        });
        expect(await outcome).toMatchObject({ code: COMMAND_ERROR_CODES.UNAVAILABLE });
        expect(screen.getByTestId('active').textContent).toBe('Plugin');
        view.unmount();
    });

    test('cancelled request cannot settle or overwrite its successor', async () => {
        const { handler, view } = setup();
        const abort = new AbortController();
        const reason = new Error('cancel');
        let cancelled: Promise<unknown> = Promise.resolve();
        let next: unknown;
        act(() => {
            cancelled = Promise.resolve(handler({ moduleId: 'Memory' }, context(abort.signal))).catch(error => error);
            // Jest's older AbortController does not retain the supplied reason.
            Object.defineProperty(abort.signal, 'reason', { configurable: true, value: reason });
            abort.abort(reason);
            next = handler({ moduleId: 'Plugin' }, context());
        });
        expect(await cancelled).toBe(reason);
        await expect(next).resolves.toEqual({ activeModule: 'Plugin' });
        expect(screen.getByTestId('active').textContent).toBe('Plugin');
        view.unmount();
    });

    test('pre-cancelled requests do not navigate and unmount releases pending work', async () => {
        const { handler, view } = setup();
        const abort = new AbortController();
        abort.abort(new Error('cancelled'));
        expect(() => handler({ moduleId: 'Memory' }, context(abort.signal))).toThrow('cancelled');
        let outcome: Promise<unknown> = Promise.resolve();
        act(() => {
            outcome = Promise.resolve(handler({ moduleId: 'Memory' }, context())).catch(error => error);
            view.unmount();
        });
        expect(await outcome).toMatchObject({ code: COMMAND_ERROR_CODES.CONNECTION_LOST });
    });
});
