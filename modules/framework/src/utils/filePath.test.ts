/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import {
    isActiveSnapshotSourceFile,
    isRelatedFilePath,
    shouldForwardPendingImportCompletion,
} from './filePath';

describe('filePath matching for memsnapshot import', () => {
    it('treats Windows slash and drive-letter case as the same file', () => {
        expect(isRelatedFilePath('C:\\Data\\Snapshot.PICKLE', 'c:/data/snapshot.pickle')).toBe(true);
    });

    it('treats a selected directory as matching a pickle under it', () => {
        expect(isRelatedFilePath('C:\\data', 'C:\\data\\snapshot.pickle')).toBe(true);
        expect(isRelatedFilePath('C:\\data\\snapshot.pickle', 'C:\\data')).toBe(true);
    });

    it('does not treat sibling paths as related', () => {
        expect(isRelatedFilePath('C:\\data\\foo', 'C:\\data\\foobar\\snapshot.pickle')).toBe(false);
        expect(isRelatedFilePath('C:\\data\\a.pickle', 'C:\\data\\b.pickle')).toBe(false);
    });

    it('only case-folds the UNC host and share', () => {
        expect(isRelatedFilePath('//Server/Share/Foo.pickle', '//server/share/Foo.pickle')).toBe(true);
        expect(isRelatedFilePath('//server/share/Foo.pickle', '//server/share/foo.pickle')).toBe(false);
    });

    it('forwards cache-hit completion while a folder import is still pending', () => {
        expect(shouldForwardPendingImportCompletion(
            { selectedFilePath: 'C:\\data', projectPath: ['C:\\data'] },
            'C:\\data\\snapshot.pickle',
        )).toBe(true);
    });

    it('drops completion that belongs to another pending project', () => {
        expect(shouldForwardPendingImportCompletion(
            { selectedFilePath: 'D:\\other.pickle', projectPath: ['D:\\other.pickle'] },
            'C:\\data\\snapshot.pickle',
        )).toBe(false);
    });

    it('matches a pending folder import and the current parse file', () => {
        expect(isActiveSnapshotSourceFile({
            toBeActivedProject: { selectedFilePath: 'C:\\data', projectPath: ['C:\\data'] },
            activeDataSource: { selectedFilePath: 'D:\\old.pickle', projectPath: ['D:\\old.pickle'] },
        }, 'C:\\data\\snapshot.pickle')).toBe(true);
        expect(isActiveSnapshotSourceFile({
            memSnapshotParseFileId: 'C:\\Data\\Snapshot.PICKLE',
        }, 'c:/data/snapshot.pickle')).toBe(true);
    });
});
