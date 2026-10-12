/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { observeTableCommands, registerTableCommands, TableControllerRegistry } from '@insight/lib/AgentTable';
import { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { registerMemScopeGraphCommands } from './graphController';
import { observeMemScopeInteractions, registerMemScopeInteractionCommands } from './interactionController';

const MODULE_ID = 'MemScope';

export const memScopeTableControllerRegistry = new TableControllerRegistry();

const moduleAgentCommandClient = new ModuleAgentCommandClient({
    moduleId: MODULE_ID,
    observe: () => ({
        moduleId: MODULE_ID,
        observedAt: Date.now(),
        tables: observeTableCommands(MODULE_ID, memScopeTableControllerRegistry),
        ...observeMemScopeInteractions(),
    }),
});

registerTableCommands(moduleAgentCommandClient, MODULE_ID, memScopeTableControllerRegistry);
registerMemScopeGraphCommands(moduleAgentCommandClient);
registerMemScopeInteractionCommands(moduleAgentCommandClient);

let stopClient: (() => void) | undefined;

export const startMemScopeAgentRuntime = (): void => {
    if (stopClient) return;
    stopClient = moduleAgentCommandClient.start();
};

export const stopMemScopeAgentRuntime = (): void => {
    stopClient?.();
    stopClient = undefined;
};
