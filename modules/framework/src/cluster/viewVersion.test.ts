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
import { ProjectType } from '../utils/enum';
import { supportsModernCluster, type ClusterDataContext } from './dataFormat';
import { CLUSTER_VERSION_KEY, ClusterViewPreference, modernClusterSource, canUseModernCluster } from './viewVersion';

const initialEnv = process.env;
beforeEach(() => { process.env = { ...initialEnv, REACT_APP_ENABLE_MODERN_CLUSTER: 'true', REACT_APP_IS_VSCODE: 'false' }; });
afterEach(() => { process.env = initialEnv; });

afterEach(() => localStorage.removeItem(CLUSTER_VERSION_KEY));
it('defaults both cluster pages to modern and restores a saved selection', () => {
    localStorage.removeItem(CLUSTER_VERSION_KEY);
    const preference = new ClusterViewPreference(); expect(preference.version).toBe('modern');
    preference.select('legacy'); expect(new ClusterViewPreference().version).toBe('legacy');
    preference.select('modern'); expect(new ClusterViewPreference().version).toBe('modern');
});
it('rejects switching during active or pending cluster comparison without changing the preference', () => {
    const preference = new ClusterViewPreference(); preference.select('legacy');
    expect(preference.select('modern', true)).toBe(false);
    preference.setComparisonPending(true); expect(preference.select('modern')).toBe(false);
    expect(new ClusterViewPreference().version).toBe('legacy');
    preference.setComparisonPending(false); expect(preference.select('modern')).toBe(true);
});
it('requires a registered modern Summary and never uses the old Summary as its replacement', () => {
    expect(modernClusterSource('Communication')).toBe('./plugins/Communication/index.html');
    expect(modernClusterSource('Summary')).toBeUndefined();
});

const source = (projectType?: ProjectType): ClusterDataContext => ({
    activeDataSource: { children: [{ type: 'CLUSTER', name: 'cluster', path: '/cluster', projectType, children: [] }] },
    clusterPageInfo: { selectedClusterPath: '/cluster' },
});
it.each([undefined, 'false', '1', 'TRUE'])('keeps the shared integration disabled for flag %s even with a saved modern preference', flag => {
    if (flag === undefined) { delete process.env.REACT_APP_ENABLE_MODERN_CLUSTER; } else {
        process.env.REACT_APP_ENABLE_MODERN_CLUSTER = flag;
    }
    localStorage.setItem(CLUSTER_VERSION_KEY, 'modern');
    const preference = new ClusterViewPreference();
    expect(preference.versionFor(source(ProjectType.DB_CLUSTER))).toBe('legacy');
    expect(canUseModernCluster(source(ProjectType.DB_CLUSTER))).toBe(false);
    expect(modernClusterSource('Summary')).toBeUndefined();
    expect(modernClusterSource('Communication')).toBeUndefined();
    expect(preference.select('modern')).toBe(false);
    expect(localStorage.getItem(CLUSTER_VERSION_KEY)).toBe('modern');
});
it.each([ProjectType.DB, ProjectType.DB_CLUSTER])('allows confirmed DB data %s without waiting for Timeline parsing', projectType => {
    expect(supportsModernCluster(source(projectType))).toBe(true);
});
it.each([ProjectType.TEXT_CLUSTER, ProjectType.TRACE, undefined])('uses legacy for unsupported or unknown format %s without changing DB preference', projectType => {
    const preference = new ClusterViewPreference(); preference.select('modern');
    expect(supportsModernCluster(source(projectType))).toBe(false);
    expect(preference.versionFor(source(projectType))).toBe('legacy');
    expect(preference.versionFor(source(ProjectType.DB_CLUSTER))).toBe('modern');
    expect(localStorage.getItem(CLUSTER_VERSION_KEY)).toBe('modern');
});
it('resolves the selected cluster and supports projects consisting of DB ranks', () => {
    const db = { type: 'CLUSTER' as const, name: 'db', path: '/cluster', projectType: ProjectType.DB_CLUSTER, children: [] };
    const text = { ...db, name: 'text', path: '/text', type: 'CLUSTER' as const, projectType: ProjectType.TEXT_CLUSTER, children: [] };
    const context = { activeDataSource: { children: [db, text] }, clusterPageInfo: { selectedClusterPath: '/text' } };
    expect(supportsModernCluster(context)).toBe(false);
    context.clusterPageInfo.selectedClusterPath = '/cluster'; expect(supportsModernCluster(context)).toBe(true);
    context.clusterPageInfo.selectedClusterPath = '/unknown'; expect(supportsModernCluster(context)).toBe(false);
    expect(supportsModernCluster({ activeDataSource: { children: [{ ...db, type: 'RANK', projectType: ProjectType.DB }] } })).toBe(true);
    expect(supportsModernCluster({})).toBe(false);
});
it('keeps an existing text cluster comparison in legacy when selecting DB data', () => {
    const preference = new ClusterViewPreference(); preference.select('modern');
    expect(preference.versionFor({ ...source(ProjectType.DB_CLUSTER), compareSet: { baseline: { projectName: 'baseline', rankId: '' } } })).toBe('legacy');
});
