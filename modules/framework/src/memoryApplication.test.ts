/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
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
