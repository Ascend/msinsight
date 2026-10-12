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
    if (error instanceof CommandError) return error.toJSON();
    if (isCommandErrorShape(error)) {
        return {
            code: error.code,
            message: error.message,
            retryable: error.retryable,
            ...(error.details === undefined ? {} : { details: error.details }),
            ...(error.state === undefined ? {} : { state: error.state }),
        };
    }
    return {
        code: COMMAND_ERROR_CODES.EXECUTION_FAILED,
        message: error instanceof Error ? error.message : String(error),
        retryable: false,
    };
};

export const isCommandErrorShape = (value: unknown): value is CommandErrorShape => {
    if (!value || typeof value !== 'object') return false;
    const error = value as Partial<CommandErrorShape>;
    return typeof error.code === 'string' && typeof error.message === 'string' && typeof error.retryable === 'boolean';
};
