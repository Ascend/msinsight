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

interface ImportFileTreeNode {
    filePath?: string;
    fileDir?: string;
    path?: string;
    children?: ImportFileTreeNode[];
}

interface RemoteImportData {
    dataSource?: {
        selectedFilePath?: string;
        projectPath?: string | string[];
    };
    importResult?: {
        isLeaks?: boolean;
        children?: ImportFileTreeNode[];
    };
}

interface MemSnapshotProgressData {
    fileId?: string;
    progress?: number;
}

const isMemSnapshotFile = (filePath: unknown): filePath is string => {
    if (typeof filePath !== 'string') {
        return false;
    }
    const lowerPath = filePath.toLowerCase();
    return lowerPath.endsWith('.pkl') || lowerPath.endsWith('.pickle');
};

const normalizeFilePath = (filePath: string): string => filePath.replace(/\\/g, '/');

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

const getImportedMemSnapshotFile = (data: RemoteImportData): string => {
    const selectedFilePath = data.dataSource?.selectedFilePath;
    if (isMemSnapshotFile(selectedFilePath)) {
        return selectedFilePath;
    }
    const projectPath = data.dataSource?.projectPath;
    if (isMemSnapshotFile(projectPath)) {
        return projectPath;
    }
    if (Array.isArray(projectPath)) {
        const filePath = projectPath.find(isMemSnapshotFile);
        if (filePath) {
            return filePath;
        }
    }
    return findMemSnapshotFile(data.importResult?.children);
};

const clearMemSnapshotParseProgress = (session: any): void => {
    session.memSnapshotParseLoading = false;
    session.memSnapshotParseProgress = 0;
    session.memSnapshotParseFileId = '';
};

const clampProgress = (progress: unknown): number => {
    if (typeof progress !== 'number' || Number.isNaN(progress)) {
        return 0;
    }
    return Math.min(100, Math.max(0, Math.round(progress)));
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
    commonRestore(session);
    funcRestore(session);
    barRestore(session);
    sliceRestore(session);
    blocksDetailsRestore(session);
    eventsDetailsRestore(session);
    detailsRestore(session);
};
export const importRemoteHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    if (!session || typeof data !== 'object') {
        return;
    }
    const fileId = getImportedMemSnapshotFile(data as RemoteImportData);
    if (!fileId) {
        runInAction(() => {
            clearMemSnapshotParseProgress(session);
        });
        return;
    }
    runInAction(() => {
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseProgress = 0;
        session.memSnapshotParseFileId = fileId;
    });
};

export const parseProgressHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    if (!session || typeof data !== 'object') {
        return;
    }
    const { fileId = '', progress } = data as MemSnapshotProgressData;
    if (!isMemSnapshotFile(fileId)) {
        return;
    }
    runInAction(() => {
        const currentFileId = session.memSnapshotParseFileId;
        if (!currentFileId || normalizeFilePath(currentFileId) !== normalizeFilePath(fileId)) {
            return;
        }
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseProgress = Math.max(session.memSnapshotParseProgress, clampProgress(progress));
    });
};

export const parseCompletedHandler = (data: any): void => {
    const session = store.sessionStore.activeSession;
    workerDestroy();
    if (session) {
        runInAction(() => {
            clearMemSnapshotParseProgress(session);
            session.deviceIds = data.deviceIds;
            session.threadIds = data.threadIds;
            session.module = data.module;
            restore(session);
        });
    }
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
            session.clickEventItem = null;
            session.leaksWorkerInfo = { ...LEAKS_WORKER_INFO_DEFAULT, renderOptions: { ...session.leaksWorkerInfo.renderOptions } };
            session.stateWorkerInfo = { ...STATE_WORKER_INFO_DEFAULT };
            session.markLineInfo = MARK_LINE_POSITION_DEFAULT;
            session.loadingBlocks = false;
            session.loadingState = false;
            clearMemSnapshotParseProgress(session);
            restore(session);
        });
    }
};

export const parseFailHandler: NotificationHandler = (data): void => {
    errorCenter.handleError(new WsError(ErrorCode.PARSE_FAIL, data.error as string));
    removeRemoteHandler(data);
};
