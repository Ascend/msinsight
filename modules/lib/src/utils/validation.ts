/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
export function isArray(value: unknown): value is unknown[] {
    return Array.isArray ? Array.isArray(value) : Object.prototype.toString.call(value) === '[object Array]';
}

/** Read an object from a response, including an empty object. Arrays are separate wire values. */
export function readRecord(value: unknown): Record<string, unknown> {
    if (value === null || typeof value !== 'object' || isArray(value)) {
        throw new Error('Invalid response object');
    }
    return value as Record<string, unknown>;
}

export function readArray(value: unknown): unknown[] {
    if (!isArray(value)) {
        throw new Error('Invalid response array');
    }
    return value;
}

export function readString(value: unknown): string {
    if (typeof value !== 'string') {
        throw new Error('Invalid string value');
    }
    return value;
}

export function readFiniteNumber(value: unknown): number {
    if (typeof value !== 'number' || !Number.isFinite(value)) {
        throw new Error('Invalid numeric value');
    }
    return value;
}

export function readSafeInteger(value: unknown, min = Number.MIN_SAFE_INTEGER): number {
    const result = readFiniteNumber(value);
    if (!Number.isSafeInteger(result) || result < min) {
        throw new Error('Invalid integer value');
    }
    return result;
}

export function readNullableNumber(value: unknown): number | null {
    return value === null || value === undefined ? null : readFiniteNumber(value);
}
