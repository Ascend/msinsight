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

export const normalizeFilePath = (filePath: string): string => {
    const normalized = filePath.replace(/\\/g, '/').replace(/\/+$/, '');
    const driveMatch = normalized.match(/^([a-zA-Z]:)(\/.*)?$/);
    if (driveMatch) {
        // Windows 本地路径整体大小写不敏感。
        return normalized.toLowerCase();
    }
    const uncMatch = normalized.match(/^(\/\/[^/]+\/[^/]+)(\/.*)?$/);
    if (uncMatch) {
        return `${uncMatch[1].toLowerCase()}${uncMatch[2] ?? ''}`;
    }
    return normalized;
};

export const isRelatedFilePath = (candidate: string, filePath: string): boolean => {
    if (!candidate || !filePath) {
        return false;
    }
    const left = normalizeFilePath(candidate);
    const right = normalizeFilePath(filePath);
    if (left === right) {
        return true;
    }
    return right.startsWith(`${left}/`) || left.startsWith(`${right}/`);
};

export const collectDataSourcePaths = (source?: {
    selectedFilePath?: string;
    projectPath?: string[] | string;
}): string[] => {
    if (!source) {
        return [];
    }
    const extra = Array.isArray(source.projectPath)
        ? source.projectPath
        : typeof source.projectPath === 'string' ? [source.projectPath] : [];
    return [source.selectedFilePath, ...extra].filter((path): path is string => typeof path === 'string' && path !== '');
};

export const isFileInDataSource = (
    source: { selectedFilePath?: string; projectPath?: string[] | string } | undefined,
    filePath: string,
): boolean => collectDataSourcePaths(source).some(path => isRelatedFilePath(path, filePath));

export const isActiveSnapshotSourceFile = (
    session: {
        toBeActivedProject?: { selectedFilePath?: string; projectPath?: string[] | string };
        activeDataSource?: { selectedFilePath?: string; projectPath?: string[] | string };
        memSnapshotParseFileId?: string;
    } | undefined,
    filePath: string,
): boolean => {
    if (!filePath || session === undefined) {
        return false;
    }
    if (isFileInDataSource(session.toBeActivedProject, filePath) || isFileInDataSource(session.activeDataSource, filePath)) {
        return true;
    }
    const parseFileId = session.memSnapshotParseFileId;
    return typeof parseFileId === 'string' && parseFileId !== '' && isRelatedFilePath(parseFileId, filePath);
};

export const shouldForwardPendingImportCompletion = (
    pendingProject: { selectedFilePath?: string; projectPath?: string[] | string } | undefined,
    dbPath: unknown,
): boolean => {
    if (pendingProject === undefined) {
        return true;
    }
    const paths = collectDataSourcePaths(pendingProject);
    if (paths.length === 0) {
        return true;
    }
    return typeof dbPath === 'string' && paths.some(path => isRelatedFilePath(path, dbPath));
};
