/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { TableCommandErrorShape, JsonObject, TableStableState } from './types';

export const TABLE_ERROR_CODES = {
    TARGET_UNAVAILABLE: 'TABLE_TARGET_UNAVAILABLE',
    NOT_READY: 'TABLE_NOT_READY',
    BUSY: 'TABLE_BUSY',
    STATE_STALE: 'TABLE_STATE_STALE',
    CAPABILITY_UNSUPPORTED: 'TABLE_CAPABILITY_UNSUPPORTED',
    COMMAND_INVALID: 'TABLE_COMMAND_INVALID',
    COLUMN_UNKNOWN: 'TABLE_COLUMN_UNKNOWN',
    FILTER_UNSUPPORTED: 'TABLE_FILTER_UNSUPPORTED',
    FILTER_VALUE_INVALID: 'TABLE_FILTER_VALUE_INVALID',
    SORT_UNSUPPORTED: 'TABLE_SORT_UNSUPPORTED',
    PAGE_OUT_OF_RANGE: 'TABLE_PAGE_OUT_OF_RANGE',
    ROW_UNKNOWN: 'TABLE_ROW_UNKNOWN',
    SELECTION_LIMIT_EXCEEDED: 'TABLE_SELECTION_LIMIT_EXCEEDED',
    EXPANSION_LIMIT_EXCEEDED: 'TABLE_EXPANSION_LIMIT_EXCEEDED',
    COMMAND_SUPERSEDED: 'TABLE_COMMAND_SUPERSEDED',
    COMMAND_TIMEOUT: 'TABLE_COMMAND_TIMEOUT',
    COMMAND_CANCELLED: 'TABLE_COMMAND_CANCELLED',
    TRANSITION_FAILED: 'TABLE_TRANSITION_FAILED',
    COPY_FAILED: 'TABLE_COPY_FAILED',
    CONNECTION_LOST: 'TABLE_CONNECTION_LOST',
} as const;

export class AgentTableError extends Error {
    readonly code: string;
    readonly retryable: boolean;
    readonly details?: JsonObject;
    readonly state?: TableStableState;

    constructor(shape: TableCommandErrorShape) {
        super(shape.message);
        this.name = 'AgentTableError';
        this.code = shape.code;
        this.retryable = shape.retryable;
        this.details = shape.details;
        this.state = shape.state;
    }

    toJSON(): TableCommandErrorShape {
        return {
            code: this.code,
            message: this.message,
            retryable: this.retryable,
            details: this.details,
            state: this.state,
        };
    }
}

export const toTableCommandError = (error: unknown): TableCommandErrorShape => {
    if (error instanceof AgentTableError) return error.toJSON();
    return {
        code: TABLE_ERROR_CODES.TRANSITION_FAILED,
        message: error instanceof Error ? error.message : String(error),
        retryable: false,
    };
};
