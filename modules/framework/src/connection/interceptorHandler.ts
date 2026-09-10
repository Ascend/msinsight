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
import { runInAction } from 'mobx';
import { getIndexByRankNameAndDeviceId, getRankInfoKey } from '@insight/lib/utils';
import type { NotificationInterceptor } from './defs';
import { type DataSource } from '@/centralServer/websocket/defs';
import { updateSession } from '@/connection/notificationHandler';
import { store } from '@/store';
import type { CardRankInfo, RankInfo } from '@/entity/session';
import { isActiveSnapshotSourceFile } from '../utils/filePath';

interface ImportActionBody {
    result: Array<{ rankId: string; dataPathList: string[] }>;
    isBinary: boolean;
    isCluster: boolean;
    isIpynb: boolean;
    isPending: boolean;
    isSimulation: boolean;
    isOnlyTraceJson: boolean;
    reset: boolean;
}

export interface ImportActionResponse {
    dataSource: DataSource;
    body: ImportActionBody;
}

interface MemoryResult {
    hasMemory: boolean;
    rankId: string; // 实际是 cardId, rankId 应该只有数字，而 cardId 可能在前面带有 host，形如: `{host} {rankId}`
    rankInfo: RankInfo;
    dbPath?: string;
    index: number;
}
interface ParseMemoryNotification {
    memoryResult: MemoryResult[];
}
interface ParseOperatorNotification {
    rankId: string; // 形如: rankName / `{host} {deviceId}`
    rankList: RankInfo[];
    dbPath?: string;
    index: number;
}
interface ParseStatisticNotification {
    rankIds: string[];
}
interface ParseLeaksNotification {
    deviceIds: object;
    threadIds: number[];
    dbPath: string;
    module: string;
    fileHash?: string;
    snapshotParsingComplete?: boolean;
    snapshotSlices?: Record<string, unknown>;
}

interface ParseHeatmapNotification {
    parseResult: boolean;
}

interface ParseMemSnapshotSliceReadyNotification {
    fileId?: string;
    fileHash?: string;
    deviceId: string;
    slice: {
        index: number;
        startEventId: number;
        endEventId: number;
        ready: boolean;
    };
}

interface ParseMemSnapshotProgressNotification {
    fileId?: string;
    progress?: number;
}

const clampMemSnapshotParseProgress = (progress: unknown): number => {
    if (typeof progress !== 'number' || Number.isNaN(progress)) {
        return 0;
    }
    return Math.min(100, Math.max(0, Math.round(progress)));
};

const areAllMemSnapshotSlicesReady = (
    snapshotSlices: Record<string, { slices?: Array<{ ready?: boolean }>; sliceCount?: number }>,
): boolean => {
    const devices = Object.values(snapshotSlices);
    return devices.length > 0 && devices.every(device =>
        (device.slices?.length ?? 0) === (device.sliceCount ?? 0) &&
        (device.slices ?? []).every(slice => slice.ready === true),
    );
};

const isActiveDataSourceFile = (filePath: string): boolean =>
    isActiveSnapshotSourceFile(store.sessionStore.activeSession, filePath);

export const parseMemorySuccessHandler: NotificationInterceptor<ParseMemoryNotification> = (data): void => {
    const session = store.sessionStore.activeSession;
    if (!session || !Array.isArray(data.memoryResult)) {
        return;
    }
    const currentCardInfos: CardRankInfo[] = [...session.memoryCardInfos];
    const memoryCardIdSet: Set<string> = new Set(currentCardInfos.map(({ rankInfo }): string => getRankInfoKey(rankInfo)));
    data.memoryResult.forEach((item) => {
        const key = getRankInfoKey(item.rankInfo);
        if (!memoryCardIdSet.has(key) && item.hasMemory) {
            currentCardInfos.push({
                rankInfo: item.rankInfo,
                dbPath: item.dbPath ?? '',
                index: getIndexByRankNameAndDeviceId(item.rankInfo.rankName, item.rankInfo.deviceId),
            });
            memoryCardIdSet.add(key);
        }
    });
    updateSession({ memoryCardInfos: currentCardInfos });
};

export const parseOperatorSuccessHandler: NotificationInterceptor<ParseOperatorNotification> = (data): void => {
    const { sessionStore } = store;
    const session = sessionStore.activeSession;
    if (!session || !Array.isArray(data.rankList)) {
        return;
    }
    const infos = [...session.operatorCardInfos];
    const keys = new Set(infos.map(({ rankInfo }) => getRankInfoKey(rankInfo)));
    data.rankList.forEach((rank) => {
        const key = getRankInfoKey(rank);
        if (keys.has(key)) {
            return;
        }
        infos.push({
            rankInfo: rank,
            dbPath: data.dbPath ?? '',
            index: getIndexByRankNameAndDeviceId(rank.rankName, rank.deviceId),
        });
        keys.add(key);
    });
    infos.sort((a, b) => a.index - b.index);
    updateSession({ operatorCardInfos: infos });
};

export const parseStatisticSuccessHandler: NotificationInterceptor<ParseStatisticNotification> = (data): void => {
    const session = store.sessionStore.activeSession;
    const iERankIds: Set<string> = new Set([...session.iERankIds]);
    data.rankIds.forEach((item) => {
        if (!iERankIds.has(item)) {
            iERankIds.add(item);
        }
    });
    updateSession({ iERankIds: [...iERankIds] });
};

export const parseLeaksSuccessHandler: NotificationInterceptor<ParseLeaksNotification> = (data): void => {
    if (data.module === 'memsnapshot' && !isActiveDataSourceFile(data.dbPath)) {
        return;
    }
    const snapshotParsingComplete = data.snapshotParsingComplete ?? true;
    const snapshotSlices = data.snapshotSlices ?? {};
    const parseUpdate: Record<string, unknown> = {
        deviceIds: data.deviceIds,
        threadIds: data.threadIds,
        dbPath: data.dbPath,
        module: data.module,
        fileHash: data.fileHash ?? '',
        snapshotParsingComplete,
        snapshotSlices,
    };
    if (data.module === 'memsnapshot') {
        const allSlicesReady = areAllMemSnapshotSlicesReady(
            snapshotSlices as Record<string, { slices?: Array<{ ready?: boolean }>; sliceCount?: number }>,
        );
        parseUpdate.memSnapshotParseFileId = data.dbPath;
        parseUpdate.memSnapshotParseLoading = !(snapshotParsingComplete && allSlicesReady);
        if (snapshotParsingComplete && allSlicesReady) {
            parseUpdate.memSnapshotParseProgress = 100;
        }
    } else {
        parseUpdate.memSnapshotParseLoading = false;
        parseUpdate.memSnapshotParseProgress = 0;
        parseUpdate.memSnapshotParseFileId = '';
    }
    updateSession(parseUpdate);
};

export const parseTritonSuccessHandler: NotificationInterceptor<ParseLeaksNotification> = (): void => {
    updateSession({ tritonParsed: true });
};

export const parseMemSnapshotProgressHandler: NotificationInterceptor<ParseMemSnapshotProgressNotification> = (data): void => {
    const session = store.sessionStore.activeSession;
    const fileId = data.fileId ?? '';
    if (!session || !isActiveDataSourceFile(fileId)) {
        return;
    }
    updateSession({
        memSnapshotParseFileId: fileId,
        memSnapshotParseLoading: true,
        memSnapshotParseProgress: Math.max(session.memSnapshotParseProgress, clampMemSnapshotParseProgress(data.progress)),
    });
};

export const parseMemSnapshotSliceReadyHandler: NotificationInterceptor<ParseMemSnapshotSliceReadyNotification> = (data): void => {
    const session = store.sessionStore.activeSession;
    if (!session || !isActiveDataSourceFile(data.fileId ?? '') || !data.fileHash || !session.fileHash ||
        data.fileHash !== session.fileHash) {
        return;
    }
    const device = session.snapshotSlices[data.deviceId];
    if (!device || data.slice.index < 0 || data.slice.index >= device.sliceCount) {
        return;
    }
    runInAction(() => {
        if (session.deviceIds[data.deviceId] === undefined) {
            session.deviceIds = { ...session.deviceIds, [data.deviceId]: ['BLOCK'] };
        }
        device.slices[data.slice.index] = { ...data.slice };
        device.readySlices = [...new Set([...device.readySlices, data.slice.index])].sort((a, b) => a - b);
        session.snapshotSlices = { ...session.snapshotSlices, [data.deviceId]: { ...device } };
        if (session.snapshotParsingComplete && areAllMemSnapshotSlicesReady(session.snapshotSlices)) {
            session.memSnapshotParseLoading = false;
            session.memSnapshotParseProgress = 100;
        }
    });
};

export const profilingExpertDataParsedHandler: NotificationInterceptor<ParseHeatmapNotification> = (data): void => {
    const session = store.sessionStore.activeSession;

    runInAction(() => {
        session.profilingExpertDataParsed = data.parseResult ?? false;
    });
};
