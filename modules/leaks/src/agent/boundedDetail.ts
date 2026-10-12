/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { JsonValue } from '@insight/lib/FrontendAgentCommand';

export interface BoundedDetailOptions {
    maxArrayItems: number;
    maxDepth?: number;
    maxObjectKeys?: number;
    maxStringLength?: number;
}

export const toBoundedDetail = (value: unknown, options: BoundedDetailOptions): JsonValue => {
    const limits = {
        maxArrayItems: options.maxArrayItems,
        maxDepth: options.maxDepth ?? 4,
        maxObjectKeys: options.maxObjectKeys ?? 50,
        maxStringLength: options.maxStringLength ?? 2000,
    };
    return visit(value, limits, 0, new WeakSet<object>());
};

const visit = (
    value: unknown,
    limits: Required<BoundedDetailOptions>,
    depth: number,
    ancestors: WeakSet<object>,
): JsonValue => {
    if (value === null || value === undefined) return null;
    if (typeof value === 'string') {
        return value.length > limits.maxStringLength
            ? `${value.slice(0, limits.maxStringLength)}...[truncated]`
            : value;
    }
    if (typeof value === 'number') return Number.isFinite(value) ? value : String(value);
    if (typeof value === 'boolean') return value;
    if (typeof value !== 'object') return String(value);
    if (depth >= limits.maxDepth) return '[max depth reached]';
    if (ancestors.has(value)) return '[circular]';

    ancestors.add(value);
    try {
        if (Array.isArray(value)) {
            const result = value.slice(0, limits.maxArrayItems).map(item => visit(item, limits, depth + 1, ancestors));
            if (value.length > limits.maxArrayItems) result.push(`[${value.length - limits.maxArrayItems} more items]`);
            return result;
        }
        const result: Record<string, JsonValue> = {};
        let count = 0;
        let omitted = 0;
        for (const key in value) {
            if (!Object.prototype.hasOwnProperty.call(value, key)) continue;
            if (count >= limits.maxObjectKeys) {
                omitted++;
                continue;
            }
            result[key] = visit((value as Record<string, unknown>)[key], limits, depth + 1, ancestors);
            count++;
        }
        if (omitted > 0) result.__truncatedFields = omitted;
        return result;
    } finally {
        ancestors.delete(value);
    }
};
