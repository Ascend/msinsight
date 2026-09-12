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
import { store } from '../store';
import { runInAction } from 'mobx';
import type { NotificationHandler } from './defs';
import i18n from '@insight/lib/i18n';
import { workerDestroy } from '@/leaksWorker/blockWorker/worker';
import { errorCenter, ErrorCode, WsError } from '@insight/lib';
import { workerDestroy as stateWorkerDestroy } from '@/leaksWorker/stateWorker/worker';
import { LEAKS_WORKER_INFO_DEFAULT, MARK_LINE_POSITION_DEFAULT, STATE_WORKER_INFO_DEFAULT } from '@/entity/session';
import type { MemSnapshotDeviceSliceInfo } from '@/entity/session';
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

const isCurrentMemSnapshotParse = (session: any, fileId: string): boolean =>
    isMemSnapshotFile(fileId) && session.memSnapshotParseFileId !== '' &&
    normalizeFilePath(fileId) === normalizeFilePath(session.memSnapshotParseFileId);

const lastMemSnapshotCompletionBySession = new WeakMap<object, any>();

const rememberMemSnapshotCompletion = (session: object, data: any): void => {
    lastMemSnapshotCompletionBySession.set(session, data);
};

const takeMatchingMemSnapshotCompletion = (session: any): any => {
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

const clearMemSnapshotParseProgress = (session: any): void => {
    session.memSnapshotParseLoading = false;
    session.memSnapshotParseProgress = 0;
    session.memSnapshotParseFileId = '';
};

const clearMemSnapshotState = (session: any): void => {
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

const finishMemSnapshotParseProgress = (session: any): void => {
    session.memSnapshotParseLoading = false;
    session.memSnapshotParseProgress = 100;
};

const initMemSnapshotParseProgress = (session: any, fileId: string): void => {
    session.memSnapshotParseLoading = true;
    session.memSnapshotParseProgress = 0;
    session.memSnapshotParseFileId = fileId;
};

const isMemSnapshotParseReady = (session: any): boolean =>
    session.snapshotParsingComplete && areAllMemSnapshotSlicesReady(session.snapshotSlices);

const resetMemSnapshotQueryState = (session: any): void => {
    workerDestroy();
    stateWorkerDestroy();
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

const beginMemSnapshotParse = (session: any, fileId: string, forceReset = false): void => {
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

const applyMemSnapshotParseProgress = (session: any, data: MemSnapshotProgressData): void => {
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

export const setTheme: NotificationHandler = (data): void => {
    window.setTheme(Boolean(data.isDark));
};

export const updateSessionHandler: NotificationHandler = (data): void => {
    const { sessionStore } = store;
    const session = sessionStore.activeSession;
    runInAction(() => {
        if (!session || typeof session !== 'object' || typeof data !== 'object') {
            return;
        }
        const dataKeys = Object.keys(data);
        const sessionKeys = Object.keys(session);
        dataKeys.forEach((key: string) => {
            if (sessionKeys.includes(key)) {
                (session as unknown as Record<string, unknown>)[key] = data[key];
            }
        });
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
    });
};

export const switchLanguageHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    const lang = data.lang as 'zhCN' | 'enUS';
    if (session) {
        runInAction(() => {
            session.language = lang;
        });
    }
    i18n.changeLanguage(lang);
};
const commonRestore = (session: any): void => {
    session.maxTime = 0;
    session.minTime = 0;
    session.legendSelect = {};
    session.synStartTime = 0;
    session.synEndTime = 0;
};
const funcRestore = (session: any): void => {
    session.funcData = { traces: [], maxTimestamp: 0, minTimestamp: 0 };
    session.threadId = '';
    session.threadOps = [];
    session.searchFunc = [];
    session.funcOptions = [];
    session.maxDepth = 0;
    session.allowTrim = true;
};
const barRestore = (session: any): void => {
    session.deviceId = '';
    session.eventType = '';
    session.deviceIdOpts = [];
    session.typeOpts = [];
    session.allocationData = { allocations: [], maxTimestamp: 0, minTimestamp: 0 };
    session.blockData = { blocks: [], minSize: 0, maxSize: 0, minTimestamp: 0, maxTimestamp: 0 };
    session.firstOffset = 0;
    session.lastOffset = 0;
    session.markLineshow = 'none';
    session.contextMenu = { visible: false, xPos: 0, yPos: 0 };
    session.allowMark = true;
    session.menuItems = [];
    session.firstLastStamps = { first: 0, last: 0 };
    session.threadFlag = false;
    session.selectedSliceIndex = -1;
    session.sliceOverviewData = {};
    session.snapshotGlobalMaxSizes = {};
};
const sliceRestore = (session: any): void => {
    session.memoryData = { size: 0, name: '', subNodes: [] };
    session.memoryStamp = 0;
};
const blocksDetailsRestore = (session: any): void => {
    session.blocksTableData = [];
    session.blocksTableHeader = [];
    session.blocksCurrentPage = 1;
    session.blocksPageSize = 10;
    session.blocksTotal = 0;
    session.blocksOrder = '';
    session.blocksOrderBy = '';
    session.blocksFilters = {};
    session.blocksRangeFilters = {};
};
const eventsDetailsRestore = (session: any): void => {
    session.eventsRangeFilters = {};
    session.eventsTableData = [];
    session.eventsTableHeader = [];
    session.eventsCurrentPage = 1;
    session.eventsPageSize = 10;
    session.eventsTotal = 0;
    session.eventsOrder = '';
    session.eventsOrderBy = '';
    session.eventsFilters = {};
};
const detailsRestore = (session: any): void => {
    session.tableType = 'blocks';
    session.tableKey = (session.tableKey + 1) % 10;
    session.lazyUsedThreshold = { perT: null, valueT: null };
    session.delayedFreeThreshold = { perT: null, valueT: null };
    session.longIdleThreshold = { perT: null, valueT: null };
    session.onlyInefficient = false;
    session.autoFilterPotentialLeaks = false;
};
const restore = (session: any): void => {
    session.clearLifecycleMemoryMarkers();
    commonRestore(session);
    funcRestore(session);
    barRestore(session);
    sliceRestore(session);
    blocksDetailsRestore(session);
    eventsDetailsRestore(session);
    detailsRestore(session);
};

const applyMemSnapshotParseCompleted = (session: any, data: any): void => {
    const parseFileId = typeof data.dbPath === 'string' ? data.dbPath : '';
    const snapshotParsingComplete = data.snapshotParsingComplete !== false;
    const incomingSlices = (data.snapshotSlices ?? {}) as Record<string, MemSnapshotDeviceSliceInfo>;
    const isSameSnapshotCompletion = data.module === 'memsnapshot' && snapshotParsingComplete &&
        session.module === 'memsnapshot' && session.fileHash !== '' && session.fileHash === data.fileHash &&
        !session.snapshotParsingComplete;
    const isFinalizationUpdate = isSameSnapshotCompletion && hasMultipleMemSnapshotSlices(incomingSlices);
    // 多分窗反插完成时取消旧分片计算；单分窗完成时保留当前已渲染结果，避免无收益的二次刷新。
    if (isFinalizationUpdate || !isSameSnapshotCompletion) {
        workerDestroy();
    }
    runInAction(() => {
        session.deviceIds = data.deviceIds;
        session.threadIds = data.threadIds;
        session.module = data.module;
        session.dbPath = parseFileId;
        session.fileHash = typeof data.fileHash === 'string' ? data.fileHash.trim() : '';
        if (!isSameSnapshotCompletion) {
            restore(session);
        }
        if (data.module !== 'memsnapshot') {
            lastMemSnapshotCompletionBySession.delete(session);
            clearMemSnapshotState(session);
            return;
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

const replayPendingMemSnapshotCompletion = (session: any): void => {
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

export const importRemoteHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
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

export const switchDirectoryHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
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

export const frameLoadedHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
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

export const parseProgressHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    if (!session || typeof data !== 'object') {
        return;
    }
    runInAction(() => {
        applyMemSnapshotParseProgress(session, data as MemSnapshotProgressData);
    });
};

export const parseCompletedHandler = (data: any): void => {
    const session = store.sessionStore.activeSession;
    const parseFileId = typeof data.dbPath === 'string' ? data.dbPath : '';
    if (!session) {
        return;
    }
    if (data.module === 'memsnapshot') {
        rememberMemSnapshotCompletion(session, data);
        if (!isCurrentMemSnapshotParse(session, parseFileId)) {
            return;
        }
    } else {
        lastMemSnapshotCompletionBySession.delete(session);
    }
    applyMemSnapshotParseCompleted(session, data);
};

export const parseSliceReadyHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
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
export const removeRemoteHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    workerDestroy();
    stateWorkerDestroy();
    if (session) {
        runInAction(() => {
            session.deviceIds = {};
            session.threadIds = [];
            session.module = 'leaks';
            session.dbPath = '';
            session.fileHash = '';
            session.snapshotParsingComplete = true;
            session.memSnapshotCacheRefreshPending = false;
            session.snapshotSlices = {};
            session.clickEventItem = null;
            session.leaksWorkerInfo = { ...LEAKS_WORKER_INFO_DEFAULT, renderOptions: { ...session.leaksWorkerInfo.renderOptions } };
            session.stateWorkerInfo = { ...STATE_WORKER_INFO_DEFAULT };
            session.markLineInfo = MARK_LINE_POSITION_DEFAULT;
            session.loadingBlocks = false;
            session.loadingOverview = false;
            session.loadingState = false;
            lastMemSnapshotCompletionBySession.delete(session);
            clearMemSnapshotParseProgress(session);
            restore(session);
        });
    }
};

export const parseFailHandler: NotificationHandler = (data): void => {
    errorCenter.handleError(new WsError(ErrorCode.PARSE_FAIL, data.error as string));
    removeRemoteHandler(data);
};
