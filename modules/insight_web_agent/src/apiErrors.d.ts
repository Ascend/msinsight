/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
export interface ErrorResponseBody {
    error?: string;
    code?: string;
    message?: string;
    messageKey?: string;
    messageParams?: Record<string, unknown>;
    details?: unknown;
    saved?: boolean;
    conflictingAgent?: string;
}
