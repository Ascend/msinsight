/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export const stableStringify = (value: unknown): string => JSON.stringify(value, (_key, nested) => {
    if (!nested || typeof nested !== 'object' || Array.isArray(nested)) return nested;
    return Object.keys(nested).sort().reduce<Record<string, unknown>>((result, key) => {
        result[key] = nested[key];
        return result;
    }, {});
}) ?? '';
