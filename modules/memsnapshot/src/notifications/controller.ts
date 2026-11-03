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
import type { MemSnapshotContext } from '../state/MemSnapshotContext';
import { restoreMemoryCommon } from '../common/shared/state/reset';
import { runInAction } from 'mobx';

import { workerDestroy } from '../common/leaksWorker/blockWorker/worker';

import { workerDestroy as stateWorkerDestroy } from '../rendering/stateWorker/worker';
import { STATE_WORKER_INFO_DEFAULT } from '../state/defaults';
import type { MemSnapshotDeviceSliceInfo } from '../state/MemSnapshotState';
import { getPreferredMemSnapshotDeviceSlices } from '../utils/memSnapshotSlices';

interface ImportFileTreeNode {
    filePath?: string;
    fileDir?: string;
    path?: string;
    children?: ImportFileTreeNode[];
}

interface RemoteImportData {
    selectedFilePath?: string;
    projectPath?: string | string[];
    dataSource?: {
        selectedFilePath?: string;
        projectPath?: string | string[];
        children?: ImportFileTreeNode[];
    };
    importResult?: {
        isLeaks?: boolean;
        children?: ImportFileTreeNode[];
    };
}

interface FrameLoadedData extends RemoteImportData {
    remoteImportData?: RemoteImportData;
}

interface MemSnapshotProgressData {
    fileId?: string;
    progress?: number;
}

interface MemSnapshotSliceReadyData {
    fileId?: string;
    fileHash?: string;
    deviceId?: string;
    slice?: {
        index: number;
        startEventId: number;
        endEventId: number;
        ready: boolean;
    };
}

const isMemSnapshotFile = (filePath: unknown): filePath is string => {
    if (typeof filePath !== 'string') {
        return false;
    }
    const lowerPath = filePath.toLowerCase();
    return lowerPath.endsWith('.pkl') || lowerPath.endsWith('.pickle');
};

const normalizeFilePath = (filePath: string): string => {
    const normalized = filePath.replace(/\\/g, '/');
    const driveMatch = normalized.match(/^([a-zA-Z]:)(\/.*)?$/);
    if (driveMatch) {
        return normalized.toLowerCase();
    }
    const uncMatch = normalized.match(/^(\/\/[^/]+\/[^/]+)(\/.*)?$/);
    if (uncMatch) {
        return `${uncMatch[1].toLowerCase()}${uncMatch[2] ?? ''}`;
    }
    return normalized;
};

const isCurrentMemSnapshotParse = (session: MemSnapshotContext, fileId: string): boolean =>
    isMemSnapshotFile(fileId) && session.memSnapshotParseFileId !== '' &&
    normalizeFilePath(fileId) === normalizeFilePath(session.memSnapshotParseFileId);

const lastMemSnapshotCompletionBySession = new WeakMap<object, any>();

const rememberMemSnapshotCompletion = (session: object, data: any): void => {
    lastMemSnapshotCompletionBySession.set(session, data);
};

const takeMatchingMemSnapshotCompletion = (session: MemSnapshotContext): any => {
    const data = lastMemSnapshotCompletionBySession.get(session);
    if (!data || typeof data.dbPath !== 'string' || !isCurrentMemSnapshotParse(session, data.dbPath)) {
        return undefined;
    }
    return data;
};

const findMemSnapshotFile = (nodes?: ImportFileTreeNode[]): string => {
    if (!Array.isArray(nodes)) {
        return '';
    }
    for (const node of nodes) {
        const filePath = [node.filePath, node.path, node.fileDir].find(isMemSnapshotFile);
        if (filePath) {
            return filePath;
        }
        const childFilePath = findMemSnapshotFile(node.children);
        if (childFilePath) {
            return childFilePath;
        }
    }
    return '';
};

const getMemSnapshotPath = (path: unknown): string => {
    if (isMemSnapshotFile(path)) {
        return path;
    }
    if (Array.isArray(path)) {
        return path.find(isMemSnapshotFile) ?? '';
    }
    return '';
};

const getImportedMemSnapshotFile = (data: RemoteImportData): string => {
    const directFilePath = getMemSnapshotPath(data.selectedFilePath) || getMemSnapshotPath(data.projectPath);
    if (directFilePath) {
        return directFilePath;
    }
    const dataSourceFile = getMemSnapshotPath(data.dataSource?.selectedFilePath) || getMemSnapshotPath(data.dataSource?.projectPath);
    if (dataSourceFile) {
        return dataSourceFile;
    }
    const dataSourceFilePath = findMemSnapshotFile(data.dataSource?.children);
    if (dataSourceFilePath) {
        return dataSourceFilePath;
    }
    return findMemSnapshotFile(data.importResult?.children);
};

const clearMemSnapshotParseProgress = (session: MemSnapshotContext): void => {
    session.memSnapshotParseLoading = false;
    session.memSnapshotParseProgress = 0;
    session.memSnapshotParseFileId = '';
};

const clearMemSnapshotState = (session: MemSnapshotContext): void => {
    session.snapshotParsingComplete = true;
    session.memSnapshotCacheRefreshPending = false;
    session.snapshotSlices = {};
    session.selectedSliceIndex = -1;
    clearMemSnapshotParseProgress(session);
};

const areAllMemSnapshotSlicesReady = (
    snapshotSlices: Record<string, MemSnapshotDeviceSliceInfo>,
): boolean => {
    const devices = Object.values(snapshotSlices);
    return devices.length > 0 && devices.every(device =>
        device.slices.length === device.sliceCount && device.slices.every(slice => slice.ready),
    );
};

export const hasMultipleMemSnapshotSlices = (
    snapshotSlices: Record<string, { sliceCount?: number } | undefined>,
): boolean => Object.values(snapshotSlices).some(device => (device?.sliceCount ?? 0) > 1);

const finishMemSnapshotParseProgress = (session: MemSnapshotContext): void => {
    session.memSnapshotParseLoading = false;
    session.memSnapshotParseProgress = 100;
};

const initMemSnapshotParseProgress = (session: MemSnapshotContext, fileId: string): void => {
    session.memSnapshotParseLoading = true;
    session.memSnapshotParseProgress = 0;
    session.memSnapshotParseFileId = fileId;
};

const isMemSnapshotParseReady = (session: MemSnapshotContext): boolean =>
    session.snapshotParsingComplete && areAllMemSnapshotSlicesReady(session.snapshotSlices);

const resetMemSnapshotQueryState = (session: MemSnapshotContext): void => {
    workerDestroy(session);
    stateWorkerDestroy(session);
    session.minTime = 0;
    session.maxTime = 0;
    session.deviceId = '';
    session.clickEventItem = null;
    session.pendingEventLocate = null;
    session.pendingBlockLocateId = null;
    if (session.leaksWorkerInfo !== undefined) {
        session.leaksWorkerInfo.clickItem = null;
        session.leaksWorkerInfo.hoverItem = null;
    }
    if (session.stateWorkerInfo !== undefined) {
        session.stateWorkerInfo.clickItem = null;
        session.stateWorkerInfo.hoverItem = null;
    }
    session.leakStats = {
        totalSize: 0,
        maxSize: 0,
        minSize: 0,
        loading: false,
        error: false,
        requestId: (session.leakStats?.requestId ?? 0) + 1,
    };
};

const beginMemSnapshotParse = (session: MemSnapshotContext, fileId: string, forceReset = false): void => {
    if (isCurrentMemSnapshotParse(session, fileId) && isMemSnapshotParseReady(session)) {
        finishMemSnapshotParseProgress(session);
        return;
    }
    if (!forceReset && isCurrentMemSnapshotParse(session, fileId)) {
        session.memSnapshotParseLoading = true;
        return;
    }
    session.snapshotParsingComplete = false;
    session.memSnapshotCacheRefreshPending = false;
    session.snapshotSlices = {};
    session.selectedSliceIndex = -1;
    session.fileHash = '';
    resetMemSnapshotQueryState(session);
    initMemSnapshotParseProgress(session, fileId);
};

const hasDeviceIds = (deviceIds: unknown): boolean => {
    if (Array.isArray(deviceIds)) {
        return deviceIds.length > 0;
    }
    if (deviceIds && typeof deviceIds === 'object') {
        return Object.keys(deviceIds).length > 0;
    }
    return false;
};

const clampProgress = (progress: unknown): number => {
    if (typeof progress !== 'number' || Number.isNaN(progress)) {
        return 0;
    }
    return Math.min(100, Math.max(0, Math.round(progress)));
};

const applyMemSnapshotParseProgress = (session: MemSnapshotContext, data: MemSnapshotProgressData): void => {
    const { fileId = '', progress } = data;
    if (!isCurrentMemSnapshotParse(session, fileId)) {
        return;
    }
    session.memSnapshotParseLoading = true;
    session.memSnapshotParseProgress = Math.max(session.memSnapshotParseProgress, clampProgress(progress));
    if (session.snapshotParsingComplete && session.memSnapshotParseProgress >= 100 &&
        areAllMemSnapshotSlicesReady(session.snapshotSlices)) {
        finishMemSnapshotParseProgress(session);
    }
};

const applyMemSnapshotParseCompleted = (session: MemSnapshotContext, data: any): void => {
    const parseFileId = typeof data.dbPath === 'string' ? data.dbPath : '';
    const snapshotParsingComplete = data.snapshotParsingComplete !== false;
    const incomingSlices = (data.snapshotSlices ?? {}) as Record<string, MemSnapshotDeviceSliceInfo>;
    const isSameSnapshotCompletion = data.module === 'memsnapshot' && snapshotParsingComplete &&
        session.module === 'memsnapshot' && session.fileHash !== '' && session.fileHash === data.fileHash &&
        !session.snapshotParsingComplete;
    const isFinalizationUpdate = isSameSnapshotCompletion && hasMultipleMemSnapshotSlices(incomingSlices);
    // 多分窗反插完成时取消旧分片计算；单分窗完成时保留当前已渲染结果，避免无收益的二次刷新。
    if (isFinalizationUpdate || !isSameSnapshotCompletion) {
        workerDestroy(session);
    }
    runInAction(() => {
        session.deviceIds = data.deviceIds;

        session.module = data.module;
        session.dbPath = parseFileId;
        session.fileHash = typeof data.fileHash === 'string' ? data.fileHash.trim() : '';
        if (!isSameSnapshotCompletion) {
            restoreMemoryCommon(session);
            restoreMemSnapshotView(session);
        }

        session.snapshotParsingComplete = snapshotParsingComplete;
        session.memSnapshotCacheRefreshPending = isFinalizationUpdate;
        if (isFinalizationUpdate) {
            session.loadedMemoryBlockContextKey = '';
            session.loadingBlocks = true;
        }
        session.snapshotSlices = data.snapshotSlices ?? {};
        if (isMemSnapshotFile(parseFileId)) {
            session.memSnapshotParseFileId = parseFileId;
        }
        if (snapshotParsingComplete && areAllMemSnapshotSlicesReady(session.snapshotSlices)) {
            finishMemSnapshotParseProgress(session);
        } else {
            session.memSnapshotParseLoading = true;
        }
        if (!isSameSnapshotCompletion) {
            const deviceSlices = getPreferredMemSnapshotDeviceSlices(session.snapshotSlices, session.deviceId);
            const readySlices = deviceSlices?.readySlices ?? [];
            session.selectedSliceIndex = readySlices.length > 0 ? Math.max(...readySlices) : -1;
        }
    });
};

const replayPendingMemSnapshotCompletion = (session: MemSnapshotContext): void => {
    const data = takeMatchingMemSnapshotCompletion(session);
    if (!data || (isMemSnapshotParseReady(session) && !session.memSnapshotParseLoading)) {
        return;
    }
    const incomingSlices = (data.snapshotSlices ?? {}) as Record<string, MemSnapshotDeviceSliceInfo>;
    const incomingReady = data.snapshotParsingComplete !== false && areAllMemSnapshotSlicesReady(incomingSlices);
    // 已在解析中时只回放全量缓存命中，避免用不完整的分窗完成事件把当前窗口冲掉。
    if (session.fileHash !== '' && !incomingReady) {
        return;
    }
    applyMemSnapshotParseCompleted(session, data);
};

export const importRemoteHandler = (session: MemSnapshotContext, data: any): void => {
    if (!session || typeof data !== 'object') {
        return;
    }
    const fileId = getImportedMemSnapshotFile(data as RemoteImportData);
    if (!fileId) {
        runInAction(() => {
            clearMemSnapshotState(session);
        });
        return;
    }
    runInAction(() => {
        beginMemSnapshotParse(session, fileId, true);
        replayPendingMemSnapshotCompletion(session);
    });
};

export const switchDirectoryHandler = (session: MemSnapshotContext, data: any): void => {
    if (!session || typeof data !== 'object') {
        return;
    }
    const fileId = getImportedMemSnapshotFile(data as RemoteImportData);
    runInAction(() => {
        if (!fileId) {
            clearMemSnapshotState(session);
            return;
        }
        beginMemSnapshotParse(session, fileId);
        replayPendingMemSnapshotCompletion(session);
    });
};

export const frameLoadedHandler = (session: MemSnapshotContext, data: any): void => {
    if (!session || typeof data !== 'object') {
        return;
    }
    if (session.memSnapshotParseLoading && session.memSnapshotParseFileId) {
        return;
    }
    if (hasDeviceIds(session.deviceIds)) {
        return;
    }
    const { remoteImportData } = data as FrameLoadedData;
    const fileId = getImportedMemSnapshotFile(remoteImportData ?? (data as RemoteImportData));
    if (!fileId) {
        return;
    }
    runInAction(() => {
        initMemSnapshotParseProgress(session, fileId);
    });
};

export const parseProgressHandler = (session: MemSnapshotContext, data: any): void => {
    if (!session || typeof data !== 'object') {
        return;
    }
    runInAction(() => {
        applyMemSnapshotParseProgress(session, data as MemSnapshotProgressData);
    });
};

export const parseCompletedHandler = (session: MemSnapshotContext, data: any): void => {
    const parseFileId = typeof data.dbPath === 'string' ? data.dbPath : '';
    if (!session) {
        return;
    }

    rememberMemSnapshotCompletion(session, data);
    if (!isCurrentMemSnapshotParse(session, parseFileId)) {
        return;
    }
    applyMemSnapshotParseCompleted(session, data);
};

export const parseSliceReadyHandler = (session: MemSnapshotContext, data: any): void => {
    const { fileId = '', fileHash = '', deviceId = '', slice } = data as MemSnapshotSliceReadyData;
    if (!session || !slice || !deviceId || !isCurrentMemSnapshotParse(session, fileId) ||
        !fileHash || !session.fileHash || fileHash !== session.fileHash) {
        return;
    }
    const device = session.snapshotSlices[deviceId];
    if (!device || slice.index < 0 || slice.index >= device.sliceCount) {
        return;
    }
    runInAction(() => {
        if (session.deviceIds[deviceId] === undefined) {
            session.deviceIds = { ...session.deviceIds, [deviceId]: ['BLOCK'] };
        }
        device.slices[slice.index] = { ...slice };
        device.readySlices = [...new Set([...device.readySlices, slice.index])].sort((a, b) => a - b);
        session.snapshotSlices = { ...session.snapshotSlices, [deviceId]: { ...device } };
        if (session.snapshotParsingComplete && areAllMemSnapshotSlicesReady(session.snapshotSlices)) {
            finishMemSnapshotParseProgress(session);
        }
    });
};
export const reconcileMemSnapshotSession = (session: MemSnapshotContext, data: any): void => {
    if (Object.prototype.hasOwnProperty.call(data, 'module') && session.module !== 'memsnapshot' &&
            session.memSnapshotParseFileId === '') {
        clearMemSnapshotState(session);
        return;
    }
    const isSnapshotStateStillArriving = !session.snapshotParsingComplete ||
            !areAllMemSnapshotSlicesReady(session.snapshotSlices);
    if (session.module === 'memsnapshot' && isSnapshotStateStillArriving &&
            session.memSnapshotParseFileId === '' && isMemSnapshotFile(session.dbPath)) {
        session.memSnapshotParseFileId = session.dbPath;
        session.memSnapshotParseLoading = true;
        if (!Object.prototype.hasOwnProperty.call(data, 'memSnapshotParseProgress')) {
            session.memSnapshotParseProgress = 0;
        }
    }
    if (session.module === 'memsnapshot' && session.selectedSliceIndex < 0) {
        const deviceSlices = getPreferredMemSnapshotDeviceSlices(session.snapshotSlices, session.deviceId);
        const readySlices = deviceSlices?.readySlices ?? [];
        if (readySlices.length > 0) {
            session.selectedSliceIndex = Math.max(...readySlices);
        }
    }
    if (session.memSnapshotParseLoading && session.snapshotParsingComplete &&
            areAllMemSnapshotSlicesReady(session.snapshotSlices)) {
        finishMemSnapshotParseProgress(session);
    }
};
const restoreMemSnapshotView = (session: MemSnapshotContext): void => {
    session.selectedSliceIndex = -1;
    session.sliceOverviewData = {};
    session.snapshotGlobalMaxSizes = {};
    session.autoFilterPotentialLeaks = false;
};
export const clearMemSnapshot = (session: MemSnapshotContext): void => {
    lastMemSnapshotCompletionBySession.delete(session);
    clearMemSnapshotState(session);
};
export const removeMemSnapshot = (session: MemSnapshotContext): void => {
    stateWorkerDestroy(session);
    clearMemSnapshot(session);
    restoreMemSnapshotView(session);
    session.clickEventItem = null;
    session.pendingEventLocate = null;
    session.stateWorkerInfo = { ...STATE_WORKER_INFO_DEFAULT };
    session.loadingState = false;
};
export const isMemSnapshotParsing = (session: MemSnapshotContext): boolean => session?.memSnapshotParseLoading;
