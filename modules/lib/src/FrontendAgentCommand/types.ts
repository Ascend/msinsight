/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export type JsonPrimitive = string | number | boolean | null;
export type JsonValue = JsonPrimitive | JsonValue[] | { [key: string]: JsonValue };
export type JsonObject = { [key: string]: JsonValue };

export interface CommandDefinition {
    name: string;
    title: string;
    description: string;
    inputSchema: JsonObject;
    approval?: 'none' | 'required';
    timeoutMs?: number;
}

export interface CommandContext {
    requestId: string;
    deadline: number;
    signal: AbortSignal;
}

export type CommandHandler = (args: JsonObject, context: CommandContext) => Promise<unknown> | unknown;

export interface CommandErrorShape {
    code: string;
    message: string;
    retryable: boolean;
    details?: JsonObject;
    state?: JsonValue;
}

export type ObservationData = Record<string, unknown>;
export type ObservationProvider = (signal: AbortSignal) => Promise<ObservationData> | ObservationData;

export const validateCommandDefinition = (definition: CommandDefinition): void => {
    if (!definition?.name || !definition.title || !definition.description) {
        throw new Error('Command definition is missing name, title, or description.');
    }
    if (!definition.inputSchema || typeof definition.inputSchema !== 'object' || Array.isArray(definition.inputSchema)) {
        throw new Error(`Command '${definition.name}' has an invalid inputSchema.`);
    }
    if (definition.approval !== undefined && definition.approval !== 'none' && definition.approval !== 'required') {
        throw new Error(`Command '${definition.name}' has an invalid approval policy.`);
    }
    if (definition.timeoutMs !== undefined && (!Number.isFinite(definition.timeoutMs) || definition.timeoutMs <= 0)) {
        throw new Error(`Command '${definition.name}' has an invalid timeoutMs.`);
    }
};
