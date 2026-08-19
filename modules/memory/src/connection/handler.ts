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
import type { MemoryRankInfo } from '../entity/memory';
import type { CardInfo, CardRankInfo } from '../entity/session';
import i18n from '@insight/lib/i18n';
import {
    customConsole as console,
    getIndexByRankNameAndDeviceId,
    getRankInfoKey,
} from '@insight/lib/utils';
import { RangeFlagList } from '../entity/memorySession';
import connector from './index';

function addMemoryCardInfos(before: CardRankInfo[], addList: MemoryRankInfo[]): CardRankInfo[] {
    const current = [...before];
    const currentKeys: Set<string> = new Set(current.map(({ rankInfo }) => getRankInfoKey(rankInfo)));
    addList.forEach((item) => {
        const key = getRankInfoKey(item.rankInfo);
        if (!currentKeys.has(key) && (item.hasMemory as boolean)) {
            current.push({
                rankInfo: item.rankInfo,
                dbPath: item.dbPath ?? '',
                index: getIndexByRankNameAndDeviceId(item.rankInfo.rankName, item.rankInfo.deviceId),
            });
            currentKeys.add(key);
        }
    });
    return current;
}

/** framework 过滤器中也有拦截，逻辑一致 {@link parseMemorySuccessHandler} */
export const parseMemoryCompletedHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        const { sessionStore } = store;
        const session = sessionStore.activeSession;
        runInAction(() => {
            if (!session) {
                return;
            }
            session.memoryCardInfos = addMemoryCardInfos(session.memoryCardInfos, data.memoryResult as MemoryRankInfo[]);
        });
    } catch (err) {
        console.error(err);
    }
};

export const removeRemoteHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        const { sessionStore, memoryStore } = store;
        const session = sessionStore.activeSession;
        const memorySession = memoryStore.activeSession;
        runInAction(() => {
            if (!session || !memorySession) {
                return;
            }
            memorySession.memoryType = '';
            session.memoryCardInfos = [];
            session.isAllMemoryCompletedSwitch = false;
            session.isCluster = false;
            session.compareRank.rankId = '';
            session.projectChangedTrigger = !session.projectChangedTrigger;
            memorySession.rankCondition = { options: [], value: 0 };
            memorySession.memoryGraphId = '';
            memorySession.rangeFlagList = [];
            memorySession.timelineOffset = 0;
        });
    } catch (error) {
        console.error(error);
    }
};

export const setTheme: NotificationHandler = (data): void => {
    window.setTheme(Boolean(data.isDark));
};

export const updateSessionHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        const { sessionStore } = store;
        const session = sessionStore.activeSession;
        runInAction(() => {
            if (!session) {
                return;
            }
            const dataKeys = Object.keys(data);
            const usableKeys: string[] = ['isCluster', 'unitcount'];
            dataKeys.forEach((key: any) => {
                if (key === 'memoryCardInfos') {
                    const memoryCardInfos = data.memoryCardInfos as CardRankInfo[];
                    // cardId 不能转成数字时排序不会报错，但是会出现逻辑错误（排序失效）
                    session.memoryCardInfos = [...memoryCardInfos].sort((a, b) => Number(a.index) - Number(b.index));
                    return;
                }
                if (usableKeys.includes(key)) {
                    (session as any)[key] = data[key];
                }
            });
        });
    } catch (error) {
        console.error(error);
    }
};

export const allSuccessHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        const { sessionStore } = store;
        const session = sessionStore.activeSession;
        runInAction(() => {
            if (!session) {
                return;
            }
            if (data.isAllPageParsed as boolean) {
                session.isAllMemoryCompletedSwitch = !session.isAllMemoryCompletedSwitch;
            }
        });
    } catch (error) {
        console.error(error);
    }
};

export const deleteCardHandler: NotificationHandler = async (data): Promise<void> => {
    try {
        const { sessionStore } = store;
        const session = sessionStore.activeSession;
        runInAction(() => {
            if (!session) {
                return;
            }
            const deleteIds: Set<string> = new Set((data.info as CardInfo[]).map(({ cardId }) => cardId));
            if (deleteIds.size > 0) {
                session.memoryCardInfos = session.memoryCardInfos.filter((item) => !deleteIds.has(item.rankInfo.rankId));
            }
        });
    } catch (error) {
        console.error(error);
    }
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

export const switchDirectoryHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    if (session) {
        runInAction(() => {
            session.compareRank = { rankId: data.rankId as string, isCompare: data.isCompare as boolean };
        });
    }
};

export const updateRangeFlagListHandler: NotificationHandler = (data): void => {
    const session = store.memoryStore.activeSession;
    if (session) {
        runInAction(() => {
            session.rangeFlagList = (data.timelineFlagList ?? []) as RangeFlagList[];
        });
    }
};

export const updateTimelineOffsetHandler: NotificationHandler = (data): void => {
    const session = store.memoryStore.activeSession;
    if (session) {
        runInAction(() => {
            session.timelineOffset = (data.offset ?? 0) as number;
        });
    }
};

export const getTimelineCardOffset = (): void => {
    const session = store.memoryStore.activeSession;
    if (session === undefined || session.rankCondition.value === undefined) {
        return;
    }
    connector.send({
        event: 'getTimelineCardOffset',
        to: 'Timeline',
        body: {
            cardId: session.selectedRankId,
            side: 'device',
        },
    });
};

const getTimelineRangeFlagList = (): void => {
    connector.send({
        event: 'getTimelineRangeFlagList',
        to: 'Timeline',
        body: {
            from: 'Memory',
        },
    });
};

export const moduleActiveHandler: NotificationHandler = (): void => {
    getTimelineRangeFlagList();
    getTimelineCardOffset();
};
