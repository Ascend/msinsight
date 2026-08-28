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
    CommandError,
    type CommandDefinition,
    type JsonObject,
} from '../FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '../ModuleAgentCommandClient';
import { registerDynamicCommands } from '../ModuleAgentCommandClient/registerDynamicCommands';
import { TABLE_COMMAND_DEFINITIONS } from './commands';
import {
    TABLE_COMMAND_IDS,
    type TableCommandId,
    type TableCommandObservation,
    type TableCommandRequest,
} from './types';
import type { TableControllerRegistry } from './TableControllerRegistry';

export const observeTableCommands = (
    moduleId: string,
    tables: Pick<TableControllerRegistry, 'observe'>,
): TableCommandObservation[] => tables.observe().map(({ capabilities, ...observation }) => ({
    ...observation,
    commands: capabilities.map(commandId => commandName(moduleId, commandId)),
}));

export const registerTableCommands = (
    client: ModuleAgentCommandClient,
    moduleId: string,
    tables: TableControllerRegistry,
): (() => void) => registerDynamicCommands(client, tables, (commandId) => {
    const definition = requireDefinition(commandId);
    return {
        definition: toCommandDefinition(moduleId, definition),
        handler: (args, context) => tables.invoke(
            toTableCommandRequest(commandId, args, context.requestId, context.deadline),
            context.signal,
        ),
    };
});

const commandName = (moduleId: string, commandId: TableCommandId): string => `${moduleId}.${commandId}`;

const toCommandDefinition = (
    moduleId: string,
    definition: typeof TABLE_COMMAND_DEFINITIONS[number],
): CommandDefinition => ({
    name: commandName(moduleId, definition.id),
    title: definition.title,
    description: definition.description ?? definition.title,
    inputSchema: withTargetSchema(definition.inputSchema),
});

const withTargetSchema = (schema: JsonObject): JsonObject => ({
    ...schema,
    ...(typeof schema.minProperties === 'number' ? { minProperties: schema.minProperties + 2 } : {}),
    ...(typeof schema.maxProperties === 'number' ? { maxProperties: schema.maxProperties + 2 } : {}),
    properties: {
        targetId: { type: 'string', minLength: 1 },
        expectedRevision: { type: 'integer', minimum: 1 },
        ...asProperties(schema.properties),
    },
    required: [...new Set(['targetId', 'expectedRevision', ...asStringArray(schema.required)])],
    additionalProperties: false,
});

const toTableCommandRequest = (
    commandId: TableCommandId,
    args: JsonObject,
    requestId: string,
    deadline: number,
): TableCommandRequest => {
    const targetId = typeof args.targetId === 'string' ? args.targetId : '';
    const expectedRevision = Number(args.expectedRevision);
    if (!targetId) throw invalid('targetId is required for table commands.');
    if (!Number.isInteger(expectedRevision) || expectedRevision < 1) {
        throw invalid('expectedRevision must be a positive integer for table commands.');
    }
    const commandArgs = { ...args };
    delete commandArgs.targetId;
    delete commandArgs.expectedRevision;
    return {
        targetId,
        expectedRevision,
        commandId,
        args: commandArgs,
        requestId,
        deadline,
    };
};

const requireDefinition = (commandId: TableCommandId): typeof TABLE_COMMAND_DEFINITIONS[number] => {
    if (!TABLE_COMMAND_IDS.includes(commandId)) throw invalid(`Unknown table command '${commandId}'.`);
    const definition = TABLE_COMMAND_DEFINITIONS.find(({ id }) => id === commandId);
    if (!definition) throw invalid(`Table command '${commandId}' has no definition.`);
    return definition;
};

const asProperties = (value: unknown): JsonObject => value && typeof value === 'object' && !Array.isArray(value)
    ? value as JsonObject
    : {};
const asStringArray = (value: unknown): string[] => Array.isArray(value)
    ? value.filter((item): item is string => typeof item === 'string')
    : [];
const invalid = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.INVALID,
    message,
    retryable: false,
});
