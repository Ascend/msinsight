/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    COMMAND_ERROR_CODES,
    FRONTEND_AGENT_EXECUTE_COMMAND,
    type CommandDefinition,
    type FrontendAgentExecuteCommandMessage,
} from '@insight/lib/FrontendAgentCommand';
import { FrontendAgentCommandController } from './FrontendAgentCommandController';

const execute = (
    controller: FrontendAgentCommandController,
    command: string,
): Promise<unknown> => (controller as unknown as {
    execute: (message: FrontendAgentExecuteCommandMessage) => Promise<unknown>;
}).execute({
    event: FRONTEND_AGENT_EXECUTE_COMMAND,
    requestId: `request-${command}`,
    command,
    args: {},
    deadline: Date.now() + 1000,
});

// Only the command attribution logic is under test, so inject a transport stub instead of a real iframe and postMessage.
const attachFakeModule = (controller: FrontendAgentCommandController, moduleId: string): void => {
    (controller as unknown as { moduleTransports: Map<string, { dispose: () => void }> })
        .moduleTransports.set(moduleId, { dispose: () => undefined });
};

const registerCommands = (controller: FrontendAgentCommandController, moduleId: string, names: string[]): void => {
    const definitions: CommandDefinition[] = names.map(name => ({
        name,
        title: name,
        description: name,
        inputSchema: { type: 'object', properties: {}, additionalProperties: false },
    }));
    (controller as unknown as { catalog: { replaceModule: (id: string, definitions: CommandDefinition[]) => void } })
        .catalog.replaceModule(moduleId, definitions);
};

describe('FrontendAgentCommandController command diagnostics', () => {
    test('explains when the active Module has not registered with the Agent framework', async () => {
        const controller = new FrontendAgentCommandController();
        attachFakeModule(controller, 'Compute');
        controller.setActiveModule('Compute');

        try {
            await expect(execute(controller, 'Compute.query')).rejects.toMatchObject({
                code: COMMAND_ERROR_CODES.UNAVAILABLE,
                details: { reason: 'module_not_registered', moduleId: 'Compute' },
            });
            await expect(execute(controller, 'Compute.query')).rejects.toThrow(
                "module 'Compute' has not registered any commands with the Agent framework",
            );
        } finally {
            controller.dispose();
        }
    });

    test('distinguishes an inactive Module command from an unknown command', async () => {
        const controller = new FrontendAgentCommandController();
        registerCommands(controller, 'Timeline', ['Timeline.zoom']);
        controller.setActiveModule('Compute');

        try {
            await expect(execute(controller, 'Timeline.zoom')).rejects.toMatchObject({
                code: COMMAND_ERROR_CODES.UNAVAILABLE,
                details: { reason: 'module_not_active', targetModule: 'Timeline', activeModule: 'Compute' },
            });
            await expect(execute(controller, 'Unknown.query')).rejects.toMatchObject({
                code: COMMAND_ERROR_CODES.NOT_FOUND,
                details: { reason: 'command_not_found' },
            });
        } finally {
            controller.dispose();
        }
    });
});
