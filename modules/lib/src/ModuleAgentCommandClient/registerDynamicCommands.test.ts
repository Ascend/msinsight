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
import type { CommandDefinition, CommandHandler } from '../FrontendAgentCommand';
import type { ModuleAgentCommandClient } from './client';
import { registerDynamicCommands, type DynamicCommandSource } from './registerDynamicCommands';

const registered = new Map<string, CommandHandler>();
let registerCount = 0;
const client = {
    registerCommand(definition: CommandDefinition, handler: CommandHandler): () => void {
        registerCount += 1;
        registered.set(definition.name, handler);
        return () => {
            registered.delete(definition.name);
        };
    },
} as unknown as ModuleAgentCommandClient;

const createSource = (initialIds: string[]): DynamicCommandSource<string> & { setCommandIds: (ids: string[]) => void; notify: () => void } => {
    let commandIds = initialIds;
    const listeners = new Set<() => void>();
    return {
        listCommandIds: () => commandIds,
        subscribeCommandsChanged: (listener: () => void) => {
            listeners.add(listener);
            return () => {
                listeners.delete(listener);
            };
        },
        setCommandIds: (ids: string[]) => {
            commandIds = ids;
        },
        notify: () => {
            listeners.forEach(listener => listener());
        },
    };
};

const definitionFor = (commandId: string): CommandDefinition => ({
    name: `Module.${commandId}`,
    title: commandId,
    description: commandId,
    inputSchema: { type: 'object', properties: {}, required: [], additionalProperties: false },
});

beforeEach(() => {
    registered.clear();
    registerCount = 0;
});

test('registers the initial command set and cleans up on dispose', () => {
    const source = createSource(['open', 'close']);
    const unregister = registerDynamicCommands(client, source, commandId => ({
        definition: definitionFor(commandId),
        handler: () => undefined,
    }));

    expect([...registered.keys()].sort()).toEqual(['Module.close', 'Module.open']);

    unregister();
    expect(registered.size).toBe(0);
});

test('re-registers when the command set changes and skips no-op notifications', () => {
    const source = createSource(['open']);
    const unregister = registerDynamicCommands(client, source, commandId => ({
        definition: definitionFor(commandId),
        handler: () => undefined,
    }));
    expect(registerCount).toBe(1);

    source.notify();
    expect(registerCount).toBe(1);

    source.setCommandIds(['open', 'select']);
    source.notify();
    expect([...registered.keys()].sort()).toEqual(['Module.open', 'Module.select']);
    expect(registerCount).toBe(3);

    source.setCommandIds([]);
    source.notify();
    expect(registered.size).toBe(0);

    unregister();
});

test('stops listening after dispose', () => {
    const source = createSource(['open']);
    const unregister = registerDynamicCommands(client, source, commandId => ({
        definition: definitionFor(commandId),
        handler: () => undefined,
    }));
    unregister();

    source.setCommandIds(['open', 'select']);
    source.notify();
    expect(registered.size).toBe(0);
});

test('binds each command id to its own handler', () => {
    const source = createSource(['a', 'b']);
    const calls: string[] = [];
    const unregister = registerDynamicCommands(client, source, commandId => ({
        definition: definitionFor(commandId),
        handler: () => {
            calls.push(commandId);
        },
    }));

    registered.get('Module.a')?.(undefined as never, {} as never);
    registered.get('Module.b')?.(undefined as never, {} as never);
    expect(calls).toEqual(['a', 'b']);

    unregister();
});
