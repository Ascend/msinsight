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

// 导入要测试的函数
import { anonymousString, handlerEmptyString } from '../string';

describe('anonymousString', () => {
    it('should return the original string if it is undefined or less than 3 characters', () => {
        expect(anonymousString('')).toBe('');
        expect(anonymousString('a')).toBe('a');
        expect(anonymousString('ab')).toBe('ab');
    });

    it('should return a string with the middle third replaced by asterisks', () => {
        expect(anonymousString('abcdef')).toBe('ab**ef');
        expect(anonymousString('abcdefghi')).toBe('abc***ghi');
    });
});

describe('handlerEmptyString', () => {
    it('should return the default value if the string is undefined or empty', () => {
        expect(handlerEmptyString('', 'default')).toBe('default');
    });

    it('should return the original string if it is not empty', () => {
        expect(handlerEmptyString('hello', 'default')).toBe('hello');
    });
});
