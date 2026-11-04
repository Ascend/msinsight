/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { CommandErrorShape } from './types';

export const COMMAND_ERROR_CODES = {
    INVALID: 'COMMAND_INVALID',
    NOT_FOUND: 'COMMAND_NOT_FOUND',
    UNAVAILABLE: 'COMMAND_UNAVAILABLE',
    BUSY: 'COMMAND_BUSY',
    TIMEOUT: 'COMMAND_TIMEOUT',
    CANCELLED: 'COMMAND_CANCELLED',
    CONNECTION_LOST: 'COMMAND_CONNECTION_LOST',
    PERMISSION_DENIED: 'COMMAND_PERMISSION_DENIED',
    APPROVAL_REQUIRED: 'COMMAND_APPROVAL_REQUIRED',
    EXECUTION_FAILED: 'COMMAND_EXECUTION_FAILED',
} as const;

export class CommandError extends Error {
    readonly code: string;
    readonly retryable: boolean;
    readonly details?: CommandErrorShape['details'];
    readonly state?: CommandErrorShape['state'];

    constructor(shape: CommandErrorShape) {
        super(shape.message);
        this.name = 'CommandError';
        this.code = shape.code;
        this.retryable = shape.retryable;
        this.details = shape.details;
        this.state = shape.state;
    }

    toJSON(): CommandErrorShape {
        return {
            code: this.code,
            message: this.message,
            retryable: this.retryable,
            ...(this.details === undefined ? {} : { details: this.details }),
            ...(this.state === undefined ? {} : { state: this.state }),
        };
    }
}

export const toCommandError = (error: unknown): CommandErrorShape => {
    if (isCommandErrorShape(error)) {
        const details = ownValue(error, 'details') as CommandErrorShape['details'];
        const state = ownValue(error, 'state') as CommandErrorShape['state'];
        return {
            code: ownValue(error, 'code') as string,
            message: ownValue(error, 'message') as string,
            retryable: ownValue(error, 'retryable') as boolean,
            ...(details === undefined ? {} : { details }),
            ...(state === undefined ? {} : { state }),
        };
    }
    return {
        code: COMMAND_ERROR_CODES.EXECUTION_FAILED,
        message: readableMessage(error),
        retryable: false,
    };
};

export const isCommandErrorShape = (value: unknown): value is CommandErrorShape => {
    if (!value || typeof value !== 'object') return false;
    return typeof ownValue(value, 'code') === 'string' && typeof ownValue(value, 'message') === 'string' && typeof ownValue(value, 'retryable') === 'boolean';
};

const ownValue = (value: unknown, key: string): unknown => {
    try {
        if (!value || typeof value !== 'object') return undefined;
        const property = Object.getOwnPropertyDescriptor(value, key);
        return property && 'value' in property ? property.value : undefined;
    } catch (_error) {
        return undefined;
    }
};

const readableMessage = (error: unknown): string => {
    const message = ownValue(error, 'message');
    if (typeof message === 'string' && message.trim()) return message;
    return typeof error === 'string' && error.trim() ? error : 'Command execution failed.';
};
