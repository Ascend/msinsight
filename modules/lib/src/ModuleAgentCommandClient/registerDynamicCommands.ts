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

export interface DynamicCommandSource<CommandId extends string> {
    listCommandIds(): CommandId[];
    subscribeCommandsChanged(listener: () => void): () => void;
}

export interface DynamicCommandBinding {
    definition: CommandDefinition;
    handler: CommandHandler;
}

export const registerDynamicCommands = <CommandId extends string>(
    client: ModuleAgentCommandClient,
    source: DynamicCommandSource<CommandId>,
    createCommand: (commandId: CommandId) => DynamicCommandBinding,
): (() => void) => {
    let unregisterCommands: Array<() => void> = [];
    let commandKey = '';

    const synchronize = (): void => {
        const commandIds = [...source.listCommandIds()].sort();
        const nextKey = commandIds.join('\n');
        if (nextKey === commandKey) return;
        commandKey = nextKey;
        unregisterCommands.forEach(unregister => unregister());
        unregisterCommands = commandIds.map((commandId) => {
            const { definition, handler } = createCommand(commandId);
            return client.registerCommand(definition, handler);
        });
    };

    const unsubscribe = source.subscribeCommandsChanged(synchronize);
    synchronize();
    return () => {
        unsubscribe();
        unregisterCommands.forEach(unregister => unregister());
        unregisterCommands = [];
    };
};
