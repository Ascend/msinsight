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
import { makeAutoObservable } from 'mobx';
import type { Cluster, Group, HostContext, LocationIntent, Scope } from '../domain/types';

export const initialContext = (): HostContext => ({ project: '', clusterPath: '', clusters: [], generation: 0, unitCount: 0, dark: true, language: 'en', compare: false });

/** One store per page instance. Query results and chart-local state stay outside this store. */
export class CommunicationStore implements HostContext {
    project = '';
    clusterPath = '';
    clusters: Cluster[] = [];
    generation = 0;
    unitCount = 0;
    dark = true;
    language: 'zh' | 'en' = 'en';
    compare = false;
    iterationId = '';
    group: Group | undefined = undefined;
    locationIntent: LocationIntent | undefined = undefined;
    private clusterContextReceived = false;
    private themeReceived = false;
    private languageReceived = false;

    constructor() {
        // Host lists and selected descriptors are immutable references, not deep observable trees.
        makeAutoObservable(this, {}, { autoBind: true, deep: false });
    }

    get activeCluster(): Cluster | undefined {
        return this.clusters.find(item => item.path === this.clusterPath);
    }

    // Defaults are not a confirmed empty selection; wait for the host's initial context and appearance.
    get initialized(): boolean { return this.clusterContextReceived && this.themeReceived && this.languageReceived; }

    get parsed(): boolean { return !this.activeCluster || this.activeCluster.parsed; }
    get durationReady(): boolean { return this.activeCluster?.durationParsed ?? false; }
    get canLocate(): boolean { return this.unitCount > 0; }

    get scope(): Scope | undefined {
        return this.clusterPath && this.iterationId && this.group
            ? { clusterPath: this.clusterPath, iterationId: this.iterationId, group: this.group }
            : undefined;
    }

    /** Host metadata changes preserve filters; project/cluster replacement invalidates all page work. */
    updateHost(patch: Partial<HostContext>): void {
        if (patch.clusters !== undefined && patch.clusterPath !== undefined) { this.clusterContextReceived = true; }
        if (patch.dark !== undefined) { this.themeReceived = true; }
        if (patch.language !== undefined) { this.languageReceived = true; }
        const changed = (patch.clusterPath !== undefined && patch.clusterPath !== this.clusterPath) ||
            (patch.project !== undefined && patch.project !== this.project);
        const generation = changed ? this.generation + 1 : patch.generation ?? this.generation;
        if (generation !== this.generation) {
            // A locate message can arrive before the initial host context.
            if (this.project !== '' || this.clusterPath !== '') { this.locationIntent = undefined; }
            this.iterationId = this.locationIntent?.iterationId ?? '';
            this.group = undefined;
        }
        Object.assign(this, patch, { generation });
    }

    resetHost(): void {
        this.locationIntent = undefined;
        this.updateHost({ ...initialContext(), dark: this.dark, language: this.language, generation: this.generation + 1 });
    }

    selectCluster(clusterPath: string): void { this.updateHost({ clusterPath }); }

    selectIteration(iterationId: string): void {
        this.locationIntent = undefined;
        if (this.iterationId !== iterationId) {
            this.iterationId = iterationId;
            this.group = undefined;
        }
    }

    selectGroup(group: Group): void {
        this.locationIntent = undefined;
        this.group = group;
    }

    /** Resolve valid selections only from results belonging to the current page generation. */
    resolveIteration(generation: number, iterations: string[]): void {
        if (generation !== this.generation) { return; }
        const iterationId = iterations.includes(this.iterationId) ? this.iterationId : iterations[0] ?? '';
        if (iterationId !== this.iterationId) {
            this.iterationId = iterationId;
            this.group = undefined;
        }
    }

    resolveGroup(generation: number, iterationId: string, groups: Group[]): void {
        if (generation !== this.generation || iterationId !== this.iterationId) { return; }
        const intent = this.locationIntent;
        const target = intent && groups.find(item => intent.groupIdHash
            ? item.groupIdHash === intent.groupIdHash
            : item.stage === intent.stage && (intent.pgName === undefined || item.pgName === intent.pgName));
        this.group = target ?? groups.find(item => item.key === this.group?.key) ?? groups[0];
    }

    /** Retained until a user changes the public filters or the host context is reset. */
    applyLocation(intent: LocationIntent): void {
        this.locationIntent = { ...intent };
        if (intent.iterationId && intent.iterationId !== this.iterationId) {
            this.iterationId = intent.iterationId;
            this.group = undefined;
        }
        if (intent.groupIdHash
            ? intent.groupIdHash !== this.group?.groupIdHash
            : intent.stage && (intent.stage !== this.group?.stage || (intent.pgName !== undefined && intent.pgName !== this.group?.pgName))) {
            this.group = undefined;
        }
    }
}
