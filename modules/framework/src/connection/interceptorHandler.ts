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
}

interface ParseHeatmapNotification {
    parseResult: boolean;
}

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
    updateSession({ deviceIds: data.deviceIds, threadIds: data.threadIds, dbPath: data.dbPath, module: data.module });
};

export const parseTritonSuccessHandler: NotificationInterceptor<ParseLeaksNotification> = (): void => {
    updateSession({ tritonParsed: true });
};

export const profilingExpertDataParsedHandler: NotificationInterceptor<ParseHeatmapNotification> = (data): void => {
    const session = store.sessionStore.activeSession;

    runInAction(() => {
        session.profilingExpertDataParsed = data.parseResult ?? false;
    });
};
