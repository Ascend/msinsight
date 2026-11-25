/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
import { getMemoryApplicationName, getMemoryApplicationSrc, modulesConfig, isMemoryApplication } from './moduleConfig';

describe('independent memory entries', () => {
    it('registers two fixed entries with independent identity and compatible backend routing', () => {
        const entries = modulesConfig.filter(config => isMemoryApplication(config.name));
        expect(entries.map(config => [config.name, config.requestName, config.attributes.src])).toEqual([
            ['MemScope', 'leaks', './plugins/MemScope/index.html'],
            ['MemSnapshot', 'leaks', './plugins/MemSnapshot/index.html'],
        ]);
        expect(getMemoryApplicationSrc('MemScope')).toBe('./plugins/MemScope/index.html');
        expect(getMemoryApplicationSrc('MemSnapshot')).toBe('./plugins/MemSnapshot/index.html');
    });
    it('selects the entry from the current file before stale parse state', () => {
        expect(getMemoryApplicationName('leaks', 'D:/data/memory.PICKLE')).toBe('MemSnapshot');
        expect(getMemoryApplicationName('memsnapshot', 'D:/data/memscope.db')).toBe('MemScope');
        expect(getMemoryApplicationName('memsnapshot')).toBe('MemSnapshot');
    });
});
