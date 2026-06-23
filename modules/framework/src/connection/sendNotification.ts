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
import connector from '@/connection/index';
import { themeInstance } from '@insight/lib/theme';
import { store } from '@/store';
import { localStorageService, LocalStorageKey } from '@insight/lib';
import { ThemeName, Language } from '@/utils/enum';
import type { LayerType } from '@/centralServer/websocket/defs';

export function sendTheme(to?: string): void {
    const isDark = themeInstance.getCurrentTheme() === ThemeName.DARK;
    connector.send({
        event: 'setTheme',
        body: { isDark },
        to,
    });
    (document.getElementById('AcpSession') as HTMLIFrameElement | null)
        ?.contentWindow
        ?.postMessage({ event: 'setTheme', body: { isDark } }, '*');
}

export function sendModuleReset(): void {
    connector.send({
        event: 'module.reset',
        body: {},
    });
}

// 数据解析状态
export function sendStatus(): void {
    const session = store.sessionStore.activeSession;
    connector.send({
        event: 'updateSession',
        body: {
            parseCompleted: session.parseCompleted,
            clusterPageInfo: session.clusterPageInfo,
            clusterList: session.clusterPageInfo.clusterList,
            selectedClusterPath: session.clusterPageInfo.selectedClusterPath,
            isFullDb: session.isFullDb,
            unitcount: session.unitcount,
            coreList: session.coreList,
            sourceList: session.sourceList,
            instrVersion: session.instrVersion,
        },
    });
}

export function sendLanguage(to?: string): void {
    connector.send({
        event: 'switchLanguage',
        to,
        body: { lang: localStorageService.getItem(LocalStorageKey.LANGUAGE) ?? Language.EN },
    });
}

export function sendWakeupPlugin(): void {
    const session = store.sessionStore.activeSession;
    connector.send({
        event: 'wakeupPlugin',
        data: { url: session.toIframeUrl },
        target: 'plugin',
    });
}

export function sendUpdateProjectName(oldProjectName: string, newProjectName: string): void {
    connector.send({
        event: 'updateProjectName',
        body: { oldProjectName, newProjectName },
    });
}

export function sendReset(): void {
    connector.send({ event: 'remote/reset', body: {}, target: 'plugin' });
}

export const sendClusterBaselineStatus = (status: boolean): void => {
    connector.send({
        event: 'clusterBaselineToggle',
        body: { status },
    });
};

export const sendShortcutKeys = (key: {hasCtrl: boolean;key: string}): void => {
    connector.send({
        event: 'keydownShortcut',
        body: key,
    });
};

export const sendDirectory = (to?: string): void => {
    const session = store.sessionStore.activeSession;
    let rankId: string = '';
    let selectedFileType: LayerType;
    let selectedFilePath: string;
    // 比对数据
    if (session.isCompareStatus) {
        rankId = session.compareSet.comparison.rankId;
        selectedFileType = session.compareSet.comparison.fileType;
        selectedFilePath = session.compareSet.comparison.filePath;
    } else {
        const { projectName } = session.activeDataSource;
        // 切换目录
        if (projectName !== '' && session.activeDataSource.selectedFilePath !== '') {
            rankId = session.activeDataSource.selectedRankId ?? '';
        }
        selectedFileType = session.activeDataSource.selectedFileType;
        selectedFilePath = session.activeDataSource.selectedFilePath;
    }
    // 通知页签
    // 联合导入时rankId可能为ProjectType.IE类型的cardId，也可能为非ProjectType.IE类型的cardId。
    // rankId在Statistic模块用来当做初始值，所以在Statistic模块需要校验iERankIds中是否有rankId，没有需默认取iERankIds[0]
    connector.send({
        event: 'switchDirectory',
        body: {
            rankId,
            selectedFileType,
            selectedFilePath,
            // 比对数据存在时，比对数据所属的项目就是 session.activeDataSource
            selectedProjectName: session.activeDataSource.projectName,
            // 比对数据存在时，比对数据所属的项目就是 session.activeDataSource，比对数据的 clusterPageInfo 就是 session.clusterPageInfo
            pageInfo: {
                cluster: session.clusterPageInfo,
                timeline: session.timelinePageInfo,
            },
            isCompare: session.isCompareStatus,
        },
        to,
    });
};

export const sendSessionKey = (key: string, to?: string): void => {
    const session = store.sessionStore.activeSession;
    const value = (session as any)[key];
    if (value === undefined) {
        return;
    }
    connector.send({
        event: 'updateSession',
        body: { [key]: value },
        to,
    });
};

export const sendMap: Record<string, (to?: string) => void> = {
    directory: sendDirectory,
    status: sendStatus,
    theme: sendTheme,
    language: sendLanguage,
    memoryRankIds: (to) => sendSessionKey('memoryRankIds', to),
    operatorRankIds: (to) => sendSessionKey('operatorRankIds', to),
    deviceIds: (to) => sendSessionKey('deviceIds', to),
    tritonParsed: (to) => sendSessionKey('tritonParsed', to),
    threadIds: (to) => sendSessionKey('threadIds', to),
    dbPath: (to) => sendSessionKey('dbPath', to),
    module: (to) => sendSessionKey('module', to),
    memoryCardInfos: (to) => sendSessionKey('memoryCardInfos', to),
    operatorCardInfos: (to) => sendSessionKey('operatorCardInfos', to),
    iERankIds: (to) => sendSessionKey('iERankIds', to),
    parseCompleted: (to) => sendSessionKey('parseCompleted', to),
};
