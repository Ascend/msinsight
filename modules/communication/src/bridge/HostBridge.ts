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
import { reaction, type IReactionDisposer } from 'mobx';
import type { HostContext, LocationIntent, Cluster } from '../domain/types';
import type { CommunicationStore } from '../store/CommunicationStore';
import type { CommunicationRequest } from '../api';
import { RequestChannel } from '@insight/lib/connection';
import { createRequest } from '@insight/lib/utils';
import { readRecord } from '@insight/lib/utils/validation';

export class HostBridge {
    private readonly requests = new RequestChannel(message => this.post(message));
    private readonly requestData = createRequest(this.requests);
    private started = false;
    private stopWatchingContext: IReactionDisposer | undefined;

    constructor(private readonly store: CommunicationStore) {}

    start(): void {
        if (this.started) { return; }
        this.started = true;
        this.stopWatchingContext = reaction(() => this.store.generation, () => {
            this.requests.cancelAll();
        });
        window.addEventListener('message', this.receive);
        this.send('communication/ready', { version: 'modern' });
        this.send('getParseStatus', {});
        this.send('getLanguage', {});
    }

    dispose(): void {
        window.removeEventListener('message', this.receive);
        this.started = false;
        this.stopWatchingContext?.();
        this.stopWatchingContext = undefined;
        this.requests.cancelAll();
    }

    send(event: string, body: Record<string, unknown>): void {
        this.post({ event, body });
    }

    private post(message: Record<string, unknown>): void {
        window.parent.postMessage(JSON.stringify({ module: 'communication', ...message, from: window.name || 'Communication' }), this.targetOrigin());
    }

    private targetOrigin(): string {
        try {
            const origin = document.referrer ? new URL(document.referrer).origin : window.location.origin;
            return origin === 'null' ? '*' : origin;
        } catch { return '*'; }
    }

    request: CommunicationRequest = (command, params, signal, module = 'communication') =>
        this.requestData(command, params, module, { signal, silent: true });

    private readonly receive = (event: MessageEvent): void => {
        // Only the owning host may update the page or resolve requests.
        if (event.source !== window.parent) { return; }
        const origin = this.targetOrigin();
        if (origin !== '*' && event.origin !== origin) { return; }
        try {
            const data = readRecord(typeof event.data === 'string' ? JSON.parse(event.data) : event.data);
            if (data.event === 'request') {
                this.requests.receive(data);
                return;
            }
            const body = readRecord(data.body ?? {});
            if (['remote/remove', 'remote/reset', 'module.reset'].includes(String(data.event))) {
                this.store.resetHost();
            } else if (data.event === 'setTheme') {
                this.store.updateHost({ dark: body.isDark === true });
            } else if (data.event === 'switchLanguage') {
                this.store.updateHost({ language: String(body.lang).startsWith('zh') ? 'zh' : 'en' });
            } else if (data.event === 'clusterBaselineToggle') {
                this.store.updateHost({ compare: body.status === true });
            } else if (['frame/loaded', 'switchDirectory', 'updateClusterPageInfo', 'updateSession'].includes(String(data.event))) {
                const pageInfo = body.pageInfo ? readRecord(body.pageInfo) : undefined;
                const cluster = pageInfo?.cluster ? readRecord(pageInfo.cluster) : body.clusterPageInfo ? readRecord(body.clusterPageInfo) : body;
                const patch: Partial<HostContext> = {};
                if (Array.isArray(cluster.clusterList)) {
                    patch.clusters = cluster.clusterList.map(raw => {
                        const item = readRecord(raw);
                        if (typeof item.path !== 'string' || typeof item.name !== 'string') { throw new Error('Invalid cluster context'); }
                        return { path: item.path, name: item.name, parsed: item.parsed === true, durationParsed: item.durationParsed === true } as Cluster;
                    });
                }
                if (typeof cluster.selectedClusterPath === 'string') { patch.clusterPath = cluster.selectedClusterPath; }
                if (typeof body.selectedProjectName === 'string') { patch.project = body.selectedProjectName; }
                const count = pageInfo?.timeline ? readRecord(pageInfo.timeline).unitCount : body.unitcount;
                if (typeof count === 'number') { patch.unitCount = count; }
                this.store.updateHost(patch);
            } else if (['locateCommunication', 'viewCommunicationDurationAnalysis'].includes(String(data.event))) {
                const intent: LocationIntent = {};
                for (const key of ['iterationId', 'stage', 'pgName', 'groupIdHash', 'operatorName'] as const) {
                    if (typeof body[key] === 'string' || (key === 'iterationId' && typeof body[key] === 'number')) { intent[key] = String(body[key]); }
                }
                this.store.applyLocation(intent);
            }
        } catch { /* Malformed or unrelated host messages must not change page state. */ }
    };
}
