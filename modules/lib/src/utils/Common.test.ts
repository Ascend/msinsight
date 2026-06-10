/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import { safeStr, formatDecimal, copyTableToClipboard } from './Common';

// 测试字符串转义方法
describe('test function safeStr', () => {
    it('test input string', () => {
        const unSafeText = 'This is unsafe / " \' & <div>div</div>';
        expect(safeStr(unSafeText)).toBe('This is unsafe &#x2F; &quot; &#39; &amp; &lt;div&gt;div&lt;&#x2F;div&gt;');
    });

    it('test input number', () => {
        const num = 100;
        expect(safeStr(num)).toBe('100');
    });

    it('test ignore', () => {
        const unSafeText = 'This is unsafe / " \' & <div>div</div>';
        expect(safeStr(unSafeText, '')).toBe('This is unsafe &#x2F; &quot; &#39; &amp; &lt;div&gt;div&lt;&#x2F;div&gt;');
        expect(safeStr(unSafeText, '<')).toBe('This is unsafe &#x2F; &quot; &#39; &amp; <div&gt;div<&#x2F;div&gt;');
        expect(safeStr(unSafeText, '>')).toBe('This is unsafe &#x2F; &quot; &#39; &amp; &lt;div>div&lt;&#x2F;div>');
        expect(safeStr(unSafeText, '&')).toBe('This is unsafe &#x2F; &quot; &#39; & &lt;div&gt;div&lt;&#x2F;div&gt;');
        expect(safeStr(unSafeText, '\'')).toBe('This is unsafe &#x2F; &quot; \' &amp; &lt;div&gt;div&lt;&#x2F;div&gt;');
        expect(safeStr(unSafeText, '"')).toBe('This is unsafe &#x2F; " &#39; &amp; &lt;div&gt;div&lt;&#x2F;div&gt;');
        expect(safeStr(unSafeText, '/')).toBe('This is unsafe / &quot; &#39; &amp; &lt;div&gt;div&lt;/div&gt;');
    });
});

describe('test function formatDecimal', () => {
    // 1. 测试默认保留位数 (fixed = 2) 的情况
    describe('Default fixed value (fixed = 2)', () => {
        // 测试整数输入
        test('should return integers with two decimal places', () => {
            expect(formatDecimal(123)).toBe(123);
            expect(formatDecimal(123.4)).toBe(123.4);
            expect(formatDecimal('456')).toBe(456);
        });

        // 测试小数的四舍五入
        test('should handle rounding correctly', () => {
            // Third digit is 5, should round up
            expect(formatDecimal(1.235)).toBe(1.24);
            // Third digit is 4, should round down
            expect(formatDecimal(1.234)).toBe(1.23);
        });

        // 测试大数截断
        test('should correctly truncate decimals beyond two places', () => {
            expect(formatDecimal(123.456)).toBe(123.46);
            expect(formatDecimal(987.65123)).toBe(987.65);
        });
    });

    // 2. 测试自定义保留位数 (fixed > 0) 的情况
    describe('Custom number of decimal places (fixed > 0)', () => {
        // 保留 3 位
        test('when fixed = 3, should keep three decimal places and round correctly', () => {
            expect(formatDecimal(1.2345, 3)).toBe(1.234);
            expect(formatDecimal(1.2344, 3)).toBe(1.234);
            expect(formatDecimal(10, 3)).toBe(10);
        });

        // 保留 0 位
        test('when fixed = 0, should return the nearest integer', () => {
            expect(formatDecimal(1.5, 0)).toBe(2);
            expect(formatDecimal(1.4, 0)).toBe(1);
            expect(formatDecimal('2.51', 0)).toBe(3);
        });
    });

    // 3. 测试处理极小（接近零）数字的情况
    describe('Handling very small numbers (near zero)', () => {
        // 应根据第一个非零数字位置动态增加保留位数
        test('should dynamically increase precision based on the first non-zero digit position', () => {
            // 0.0123: decimal = 2. fixed = 2. decimal <= fixed. Keep fixed = 2 places.
            expect(formatDecimal(0.0123)).toBe(0.01);

            // 0.00123: decimal = 3. fixed = 2. decimal > fixed. Keep 3 - 1 + 2 = 4 places.
            expect(formatDecimal(0.00123)).toBe(0.0012);

            // 0.00125: Should round up to 4 places.
            expect(formatDecimal(0.00125, 2)).toBe(0.0013);

            // 0.000005: decimal = 6. fixed = 2. Keep 6 - 1 + 2 = 7 places.
            expect(formatDecimal(0.000005)).toBe(0.000005);
        });

        // 应正确处理极小数字与自定义 fixed 的组合
        test('should handle small numbers with custom fixed combination', () => {
            // 0.000123: decimal = 4. fixed = 1. Keep 4 - 1 + 1 = 4 places.
            expect(formatDecimal(0.000123, 1)).toBe(0.0001);

            // 0.0123: decimal = 2. fixed = 5. decimal <= fixed. Keep fixed = 5 places.
            expect(formatDecimal(0.0123, 5)).toBe(0.0123);
        });
    });

    // 4. 测试边界条件和特殊输入
    describe('Boundary conditions and special inputs', () => {
        // 测试零值
        test('should return 0 when input is 0 or "0"', () => {
            expect(formatDecimal(0)).toBe(0);
            expect(formatDecimal('0')).toBe(0);
        });

        // 测试负数
        test('should correctly handle negative numbers', () => {
            // Negative number rounding
            expect(formatDecimal(-1.235)).toBe(-1.24);
            // Negative small number handling (decimal = 4, fixed = 2, keep 5 places)
            expect(formatDecimal(-0.00123)).toBe(-0.0012);
        });

        // 测试非法输入（NaN）
        test('should return NaN for non-numeric or NaN inputs', () => {
            expect(formatDecimal('abc')).toBeNaN();
            expect(formatDecimal(NaN)).toBeNaN();
        });
    });
});

describe('test function copyTableToClipboard', () => {
    const writeText = jest.fn();

    beforeEach(() => {
        writeText.mockResolvedValue(undefined);
        Object.assign(navigator, {
            clipboard: { writeText },
        });
    });

    afterEach(() => {
        writeText.mockClear();
    });

    it('should pass the actual row index to column renderers', async() => {
        const columns = [
            {
                title: 'Index',
                dataIndex: 'index',
                render: (_value: unknown, _record: unknown, index: number) => index + 1,
            },
            {
                title: 'Name',
                dataIndex: 'name',
            },
        ];
        const dataSource = [
            { name: 'first' },
            { name: 'second' },
            { name: 'third' },
        ];

        await copyTableToClipboard(columns, dataSource);

        expect(writeText).toHaveBeenCalledWith('Index\tName\t\n1\tfirst\t\n2\tsecond\t\n3\tthird\t\n');
    });
});
