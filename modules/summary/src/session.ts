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
import { makeAutoObservable, runInAction } from 'mobx';
import type { ClientConnector } from '@insight/lib/connection';
import type { TimeField } from './api';

export interface ClusterInfo {
    name: string;
    path: string;
    parsed: boolean;
    durationParsed: boolean;
}

export interface ClusterPageInfo {
    clusterList: ClusterInfo[];
    selectedClusterPath: string;
}

// Owned by Overview: never import or mutate the legacy Cluster session.
export class OverviewSession {
    clusterList: ClusterInfo[] = [];
    selectedClusterPath = '';
    selectedProjectName = '';
    summaryStep = 'All';
    selectedTimeField: TimeField | undefined = undefined;
    stepList: string[] = [];
    renderId = 0;
    isCompare = false;
    language: 'enUS' | 'zhCN' = 'enUS';

    constructor() {
        makeAutoObservable(this);
    }

    get ready(): boolean {
        const cluster = this.clusterList.find(cluster => cluster.path === this.selectedClusterPath);
        return !this.isCompare && !!cluster?.parsed && !!cluster?.durationParsed;
    }

    resetForClusterChange(): void {
        this.summaryStep = 'All';
        this.selectedTimeField = undefined;
        this.stepList = [];
        this.renderId++;
    }

    selectCluster(path: string): void {
        if (path !== this.selectedClusterPath) {
            this.resetForClusterChange();
            this.selectedClusterPath = path;
        }
    }

    selectTimeField(field: TimeField): void {
        this.selectedTimeField = this.selectedTimeField === field ? undefined : field;
    }

    updateClusters(info: ClusterPageInfo): void {
        // Copy host data so this module never shares mutable selection/state with another view.
        this.clusterList = info.clusterList.map(cluster => ({ ...cluster }));
        this.selectCluster(info.selectedClusterPath);
    }

    reset(): void {
        this.resetForClusterChange();
        this.clusterList = [];
        this.selectedClusterPath = '';
        this.selectedProjectName = '';
        this.isCompare = false;
    }
}

type Body = Record<string, unknown>;

function clusterPage(value: unknown, current?: ClusterPageInfo): ClusterPageInfo | undefined {
    if (!value || typeof value !== 'object' || Array.isArray(value)) return undefined;
    const page = { ...current, ...value } as Partial<ClusterPageInfo>;
    if (typeof page.selectedClusterPath !== 'string' || !Array.isArray(page.clusterList)) return undefined;
    if (!page.clusterList.every(cluster => cluster && typeof cluster.name === 'string' && typeof cluster.path === 'string' &&
        typeof cluster.parsed === 'boolean' && typeof cluster.durationParsed === 'boolean')) return undefined;
    return page as ClusterPageInfo;
}

export function connectOverviewContext(
    session: OverviewSession,
    connector: Pick<ClientConnector, 'addListener' | 'removeListener' | 'send'>,
    setTheme: (dark: boolean) => void,
    setLanguage: (language: 'enUS' | 'zhCN') => void,
): () => void {
    const directory = (body: Body): void => {
        if (typeof body.selectedProjectName === 'string' && session.selectedProjectName !== body.selectedProjectName) {
            session.reset();
            session.selectedProjectName = body.selectedProjectName;
        }
        const page = clusterPage((body.pageInfo as { cluster?: unknown } | undefined)?.cluster);
        if (page) session.updateClusters(page);
    };
    const language = (body: Body): void => {
        if (body.lang === 'enUS' || body.lang === 'zhCN') {
            session.language = body.lang;
            setLanguage(body.lang);
        }
    };
    const handlers: Record<string, (body: Body) => void> = {
        'frame/loaded': directory,
        switchDirectory: directory,
        updateClusterPageInfo: body => {
            const page = clusterPage(body);
            if (page) session.updateClusters(page);
        },
        updateSession: body => {
            const page = clusterPage(body.clusterPageInfo, {
                clusterList: session.clusterList, selectedClusterPath: session.selectedClusterPath,
            });
            if (page) session.updateClusters(page);
        },
        switchLanguage: language,
        setTheme: body => { if (typeof body.isDark === 'boolean') setTheme(body.isDark); },
        clusterBaselineToggle: body => {
            if (typeof body.status === 'boolean' && session.isCompare !== body.status) {
                session.isCompare = body.status;
                session.resetForClusterChange();
            }
        },
        'remote/remove': () => session.reset(),
        'remote/reset': () => session.reset(),
        'module.reset': () => session.reset(),
    };
    const listeners = Object.entries(handlers).map(([event, handler]) => connector.addListener(event, message => {
        const body = message.data?.body;
        if (body && typeof body === 'object' && !Array.isArray(body)) {
            runInAction(() => handler(body));
        }
    }));
    connector.send({ event: 'clusterPage/ready', body: { version: 'modern' } });
    return () => { listeners.forEach(listener => connector.removeListener(listener)); };
}
