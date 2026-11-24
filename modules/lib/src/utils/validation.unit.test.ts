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
import { readArray, readFiniteNumber, readNullableNumber, readRecord, readSafeInteger, readString } from '@insight/lib/utils/validation';

it('accepts empty containers and returns the original values', () => {
    const record = {};
    const list: unknown[] = [];
    expect(readRecord(record)).toBe(record);
    expect(readArray(list)).toBe(list);
    expect(readString('')).toBe('');
    expect(() => readRecord(list)).toThrow();
    expect(() => readRecord(null)).toThrow();
    expect(() => readArray(record)).toThrow();
    expect(() => readString(1)).toThrow();
});

it('preserves zero and only maps missing nullable numbers to null', () => {
    expect(readNullableNumber(undefined)).toBeNull();
    expect(readNullableNumber(null)).toBeNull();
    expect(readNullableNumber(0)).toBe(0);
    for (const value of ['0', NaN, Infinity, -Infinity]) {
        expect(() => readNullableNumber(value)).toThrow();
    }
    expect(() => readFiniteNumber(null)).toThrow();
});

it('checks the safe integer range and an inclusive lower bound', () => {
    expect(readSafeInteger(Number.MIN_SAFE_INTEGER)).toBe(Number.MIN_SAFE_INTEGER);
    expect(readSafeInteger(1, 1)).toBe(1);
    expect(() => readSafeInteger(0, 1)).toThrow();
    expect(() => readSafeInteger(1.5)).toThrow();
    expect(() => readSafeInteger(Number.MAX_SAFE_INTEGER + 1)).toThrow();
});
