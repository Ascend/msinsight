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

import { getAutoKey, AutoKey } from '../dataAutoKey';

describe('getAutoKey', () => {
    it('should return a unique key for each object', () => {
        const obj1: AutoKey<{}> = {};
        const obj2: AutoKey<{}> = {};
        const key1 = getAutoKey(obj1);
        const key2 = getAutoKey(obj2);
        expect(key1).not.toEqual(key2);
    });

    it('should return the same key for the same object', () => {
        const obj: AutoKey<{}> = {};
        const key1 = getAutoKey(obj);
        const key2 = getAutoKey(obj);
        expect(key1).toEqual(key2);
    });

    it('does not confuse an unrelated symbol with the internal key', () => {
        const obj: AutoKey<{}> = { [Symbol('autokey')]: 'customKey' };
        const key = getAutoKey(obj);
        expect(key).not.toBe('customKey');
        expect(getAutoKey(obj)).toBe(key);
    });
});
