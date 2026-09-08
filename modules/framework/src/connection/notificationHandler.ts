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
import { store } from '@/store';
import type { CardInfo, Session } from '@/entity/session';
import connector from '@/connection';
import { firstLetterUpper } from '@/utils';
import {
    sendMap,
    sendLanguage,
    sendStatus,
    sendTheme,
} from './sendNotification';
import { NotificationMessage } from './notification';
import { SessionAction } from '@/utils/enum';
import { customConsole as console } from '@insight/lib/utils';

export const updateSessionHandler = (e: NotificationMessage): void => {
    const session = store.sessionStore.activeSession;
    const receiver = e.data.body;
    if (receiver === undefined) {
        console.warn('data.body is undefined, please check your params');
    }
    const updateState = updateSession(receiver);

    if (updateState.unitcount) {
        session.timelinePageInfo.unitCount = updateState.unitcount;
    }

    if (updateState.timeAnalysisRange !== undefined) {
        connector.send({
            event: 'updateSession',
            body: { timeAnalysisRange: session.timeAnalysisRange },
            to: 'NUMA',
        });
    }

    setTimeout(() => {
        const isSend =
            (updateState.parseCompleted !== undefined ||
                updateState.unitcount !== undefined ||
                updateState.isBinary === true ||
                updateState.durationFileCompleted === true ||
                updateState.isIpynb === true ||
                updateState.ipynbUrl !== '') &&
            receiver.broadcast !== false;
        if (isSend) {
            connector.send({
                event: 'updateSession',
                body: {
                    parseCompleted: session.parseCompleted,
                    unitcount: session.unitcount,
                    instrVersion: session.instrVersion,
                    ...updateState,
                },
            });
        }
    });
};

export const updateSession = (receiver: Record<string, any>): Record<string, any> => {
    if (receiver === undefined || receiver == null) {
        return {};
    }
    const session = store.sessionStore.activeSession;
    const receiverPropKeys = Object.keys(receiver);
    const sessionPropKeys = Object.keys(session);
    const updateState: Record<string, any> = {};
    for (const key of receiverPropKeys) {
        // 1.receiver的字段key在session中存在
        // 2.receiver[key]的类型（例如string、boolean)与session[key]也相同，或者session[key]当前为null
        const isSameType = Object.prototype.toString.call(receiver[key]) === Object.prototype.toString.call(session[key as keyof Session]);
        const isTimeAnalysisRange = key === 'timeAnalysisRange' &&
            (receiver[key] === null || (Array.isArray(receiver[key]) && receiver[key].length === 2));
        const valid = sessionPropKeys.includes(key) &&
            (isSameType || session[key as keyof Session] === null || isTimeAnalysisRange);
        if (valid) {
            Object.assign(updateState, { [key]: receiver[key] });
        } else {
            console.warn(`you just send a invalid data: {${key}: ${receiver[key]}} to update session, please check it`);
        }
    }
    runInAction(() => {
        Object.entries(updateState).forEach(([key, value]) => {
            (session as any)[key] = value;
        });
    });
    return updateState;
};

export const getParseStatusHandler = (e: NotificationMessage): void => {
    const session = store.sessionStore.activeSession;
    // 请求特定数据
    const receiver = e.data.body;
    const requestList = receiver?.requests as string[];
    if (requestList?.length > 0) {
        requestList.forEach(key => {
            sendMap[key]?.(e.data.from);
        });
        return;
    }
    const requestKey = receiver?.request as string;
    if (requestKey !== undefined && requestKey !== null && (session as any)[requestKey] !== undefined) {
        connector.send({
            event: 'updateSession',
            body: { [requestKey]: (session as any)[requestKey] },
            to: receiver?.from === undefined ? undefined : firstLetterUpper(receiver?.from),
        });
        return;
    }
    // 请求通用数据
    sendStatus();
    // 主题
    sendTheme();
};

export const getThemeHandler = (): void => {
    sendTheme();
};

// 接收从 timeline removeSingleRemoteHandler 传来的删除卡消息
export const deleteCardHandler = (e: NotificationMessage): void => {
    const receiver = e.data.body;
    if (receiver === null || receiver === undefined) {
        console.warn('data.body is undefined, please check your params');
        return;
    }
    connector.send({ event: 'deleteCard', body: receiver });
    const removeCardInfos = receiver.info as CardInfo[];
    const removeCardIds: Set<string> = new Set(removeCardInfos.map(({ cardId }) => cardId));
    if (removeCardIds.size > 0) {
        const session = store.sessionStore.activeSession;
        const memoryCardInfos = session.memoryCardInfos.filter((item) => !removeCardIds.has(item.rankInfo.rankId));
        const operatorCardInfos = session.operatorCardInfos.filter((item) => !removeCardIds.has(item.rankInfo.rankId));
        const iERankIds = session.iERankIds.filter((item) => !removeCardIds.has(item));
        updateSession({ memoryCardInfos, operatorCardInfos, iERankIds });
    }
};

export const switchModuleHandler = (e: NotificationMessage): void => {
    const session = store.sessionStore.activeSession;
    const body = e.data.body;
    if (body?.switchTo === null || body?.switchTo === undefined) {
        return;
    }
    const moduleName = firstLetterUpper(body.switchTo);
    // 切换到此模块
    runInAction(() => {
        session.actionListener = { type: SessionAction.SWITCH_ACTIVE_MODULE, value: moduleName };
    });
    if (body.toModuleEvent !== undefined) {
        // 向目标模块发送消息
        connector.send({
            event: body.toModuleEvent,
            // 广播到所有模块，还是只目标模块
            to: body.broadcast ? undefined : moduleName,
            body: body.params,
        });
    }
};

export const getLanguageHandler = (e: NotificationMessage): void => {
    sendLanguage(e.data.from);
};

export const openImportDialogHandler = (): void => {
    const session = store.sessionStore.activeSession;

    session.actionListener = { type: SessionAction.IMPORT_MOE_LOAD_DATA, value: '' };
};

export const clusterCompletedHandler = (e: any): void => {
    const data = e.data.body;
    const session = store.sessionStore.activeSession;
    runInAction(() => {
        session.setClusterParsed(data.clusterPath);
        connector.send({ // 初始加载 Communication Summary 页面时无法未注册监听事件，因此无法接收，这段代码用处在切换集群的情况
            event: 'updateClusterPageInfo',
            body: session.clusterPageInfo,
        });
    });
};

export const clusterDurationCompletedHandler = (e: any): void => {
    const data = e.data.body;
    const session = store.sessionStore.activeSession;
    runInAction(() => {
        session.setClusterDurationParsed(data.clusterPath);
        if (data.isCluster !== undefined) {
            session.isCluster = data.isCluster;
        }
        connector.send({ // 初始加载 Communication Summary 页面时无法未注册监听事件，因此无法接收，这段代码用处在切换集群的情况
            event: 'updateClusterPageInfo',
            body: session.clusterPageInfo,
        });
    });
};
