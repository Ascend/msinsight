/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export const errorResult = (code, message, status, details) => ({
    error: code,
    message,
    status,
    ...(details === undefined ? {} : { details }),
});

export const errorCause = (error) => error instanceof Error ? error.message : String(error);
