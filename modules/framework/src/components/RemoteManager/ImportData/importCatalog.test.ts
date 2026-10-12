/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import {
    catalogReloadKind,
    parentDirectory,
    resolveSortFocus,
    shouldApplyUserSort,
    trimFocusPath,
} from './importCatalog';

describe('import catalog sort focus', () => {
    it('keeps drive roots and strips trailing separators', () => {
        expect(trimFocusPath('C:')).toBe('C:');
        expect(trimFocusPath('C:\\')).toBe('C:\\');
        expect(trimFocusPath('/data/proj/')).toBe('/data/proj');
        expect(trimFocusPath('')).toBe('');
    });

    it('resolves a file focus to its parent directory', () => {
        expect(parentDirectory('C:\\data\\trace.json')).toBe('C:\\data');
        expect(parentDirectory('C:\\trace.json')).toBe('C:\\');
        expect(parentDirectory('/data/trace.json')).toBe('/data');
        expect(parentDirectory('/trace.json')).toBe('/');
        expect(resolveSortFocus('/data/trace.json', true)).toBe('/data');
        expect(resolveSortFocus('/data/proj', false)).toBe('/data/proj');
    });

    it('applies user sort to a file parent but not to ancestors', () => {
        expect(shouldApplyUserSort('C:\\data', 'C:\\data\\trace.json', true)).toBe(true);
        expect(shouldApplyUserSort('C:\\', 'C:\\data\\trace.json', true)).toBe(false);
        expect(shouldApplyUserSort('C:\\data', 'C:\\data\\trace.json', false)).toBe(false);
        expect(shouldApplyUserSort('/data/proj', '/data/proj', false)).toBe(true);
        expect(shouldApplyUserSort('/data/proj/run', '/data/proj', false)).toBe(true);
        expect(shouldApplyUserSort('/data', '/data/proj', false)).toBe(false);
        expect(shouldApplyUserSort('D:\\', 'D:\\zhang', false)).toBe(false);
        expect(shouldApplyUserSort('D:', 'D:\\zhang', false)).toBe(false);
        expect(shouldApplyUserSort('D:\\zhang', 'D:\\zhang', false)).toBe(true);
        expect(shouldApplyUserSort('/other', '', false)).toBe(true);
    });
});

describe('import catalog reload', () => {
    const base = { prevNonce: 1, nonce: 1, type: 'search', prevSortSig: 'name:asc', sortSig: 'name:asc' };

    it('keeps an explicit refresh when only the nonce changes', () => {
        expect(catalogReloadKind({ ...base, nonce: 2 })).toBe('search');
        expect(catalogReloadKind({ ...base, nonce: 2, type: 'refresh' })).toBe('search');
        expect(catalogReloadKind(base)).toBe('none');
    });

    it('reloads sort without treating an unchanged search as a new query', () => {
        expect(catalogReloadKind({ ...base, sortSig: 'mtime:desc' })).toBe('sort');
        expect(catalogReloadKind({ ...base, type: 'input path change', nonce: 2, sortSig: 'mtime:desc' })).toBe('sort');
    });
});
