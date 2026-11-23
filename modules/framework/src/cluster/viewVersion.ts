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
import { clusterNavigation } from './navigation';
import { supportsModernCluster, type ClusterDataContext } from './dataFormat';
import { makeAutoObservable } from 'mobx';
import { modulesConfig } from '../moduleConfig';
export type ClusterViewVersion = 'modern' | 'legacy';
export const CLUSTER_VERSION_KEY = 'insight.cluster.viewVersion';
// VSCode keeps its existing injected HTML loader until modern pages have a plugin adapter.
export const isModernClusterEnabled = (): boolean => process.env.REACT_APP_IS_VSCODE !== 'true';
export const canUseModernCluster = (context: ClusterDataContext): boolean => isModernClusterEnabled() && supportsModernCluster(context);
export class ClusterViewPreference {
    version: ClusterViewVersion = 'legacy';
    comparisonPending = false;
    constructor() {
        try { if (localStorage.getItem(CLUSTER_VERSION_KEY) === 'modern') { this.version = 'modern'; } } catch { /* Storage may be unavailable in embedded hosts. */ }
        makeAutoObservable(this);
    }

    versionFor(context: ClusterDataContext): ClusterViewVersion {
        const baseline = context.compareSet?.baseline;
        const comparing = !!baseline?.projectName && baseline.rankId === '';
        return canUseModernCluster(context) && !comparing ? this.version : 'legacy';
    }

    setComparisonPending(pending: boolean): void { this.comparisonPending = pending; }

    select(version: ClusterViewVersion, clusterComparisonActive = false): boolean {
        if (version === 'modern' && (!isModernClusterEnabled() || clusterComparisonActive || this.comparisonPending)) { return false; }
        if (this.version !== version) { clusterNavigation.reset(); }
        this.version = version;
        try { localStorage.setItem(CLUSTER_VERSION_KEY, version); } catch { /* Keep an in-memory preference. */ }
        return true;
    }
}
export const clusterView = new ClusterViewPreference();
export const isClusterApplication = (name: string): boolean => name === 'Summary' || name === 'Communication';
/** Summary uses its registered entry unless a deployment explicitly overrides it. */
export function modernClusterSource(name: string): string | undefined {
    if (!isModernClusterEnabled()) { return undefined; }
    const development = process.env.REACT_APP_ENV === 'development';
    if (name === 'Communication') { return development ? 'http://localhost:3012/' : './plugins/Communication/index.html'; }
    const configured = modulesConfig.find(config => config.name === 'Summary')?.modernSrc;
    const src = process.env.REACT_APP_SUMMARY_MODERN_SRC ?? configured;
    if (!src) { return undefined; }
    if (src.startsWith('./plugins/') && !src.includes('..')) { return src; }
    if (development && /^http:\/\/(localhost|127\.0\.0\.1):\d+\//.test(src)) { return src; }
    return undefined;
}
