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
import { setBaselineData, setCompareData, isInClusterCompare } from '../utils/Compare';
import React from 'react';
import { fireEvent } from '@testing-library/react';
import { message } from 'antd';
import { renderWithProviders } from '../../../testing/render';
import ContextMenu from '../components/RemoteManager/ProjectContents/ContextMenu';
import type { Session } from '../entity/session';
import { ProjectType } from '../utils/enum';
import { clusterView } from './viewVersion';
import { store } from '../store';
import { cancelBaseline, setBaseline } from '../utils/Request';

const initialEnv = process.env;
beforeEach(() => { process.env = { ...initialEnv, REACT_APP_IS_VSCODE: 'false' }; });
afterEach(() => { process.env = initialEnv; });

jest.mock('@/connection', () => ({ __esModule: true, default: { send: jest.fn() } }));
jest.mock('@insight/lib/i18n', () => ({ __esModule: true, default: { t: (key: string) => key } }));
jest.mock('antd', () => ({ message: { info: jest.fn(), warning: jest.fn() } }));
jest.mock('@/connection/sendNotification', () => ({ sendClusterBaselineStatus: jest.fn() }));
jest.mock('@/utils/Request', () => ({ setBaseline: jest.fn(), cancelBaseline: jest.fn() }));
jest.mock('@/utils/Rank', () => ({ getRankInfo: jest.fn() }));
jest.mock('@/store', () => ({ store: { sessionStore: { activeSession: { activeDataSource: { projectName: 'current' }, isCluster: true, compareSet: { baseline: { projectName: '', filePath: '', rankId: '' }, comparison: {} } } } } }));
const input = { projectName: 'baseline', fileType: 'PROJECT' as const, filePath: '/baseline', rankId: '', baselineClusterPath: '/baseline', currentClusterPath: '/current' };
beforeEach(() => {
    store.sessionStore.activeSession.activeDataSource.children = [{ type: 'CLUSTER', name: 'current', path: '/current', projectType: ProjectType.DB_CLUSTER, children: [] }];
    clusterView.setComparisonPending(false); clusterView.select('modern');
    (cancelBaseline as jest.Mock).mockResolvedValue({});
    (setBaseline as jest.Mock).mockResolvedValue({ isCluster: true });
    Object.assign(store.sessionStore.activeSession.compareSet, { baseline: { projectName: '', filePath: '', rankId: '' }, comparison: {} });
});
it('rejects modern cluster baseline before cancelling an existing baseline or making backend calls', async () => {
    await setBaselineData(input);
    expect(message.warning).toHaveBeenCalledWith('clusterView.compareUnavailable');
    expect(cancelBaseline).not.toHaveBeenCalled(); expect(setBaseline).not.toHaveBeenCalled();
});
it('keeps the cluster context menu action enabled and warns on click without changing comparison state', () => {
    const session = Object.assign(store.sessionStore.activeSession, {
        selectedFile: { ...input },
        contextMenu: { visible: true },
        dataSources: [],
        getClusterPath: () => '/current',
        getDataSourceByProjectName: () => ({ children: [{ type: 'CLUSTER', path: '/baseline' }] }),
    });
    const before = JSON.stringify(session.compareSet);
    const view = renderWithProviders(React.createElement(ContextMenu, { session: session as unknown as Session }));
    const action = view.getByText('Set as Baseline Data');
    expect(action).not.toHaveClass('disabled');
    expect(message.warning).not.toHaveBeenCalled();
    fireEvent.click(action);
    expect(message.warning).toHaveBeenCalledWith('clusterView.compareUnavailable');
    expect(setBaseline).not.toHaveBeenCalled();
    expect(cancelBaseline).not.toHaveBeenCalled();
    expect(JSON.stringify(session.compareSet)).toBe(before);
    expect(session.contextMenu.visible).toBe(false);
});
it('preserves legacy cluster comparison', async () => {
    clusterView.select('legacy'); await setBaselineData(input);
    expect(setBaseline).toHaveBeenCalledWith(input); expect(isInClusterCompare()).toBe(true);
    expect(clusterView.comparisonPending).toBe(false);
});
it('preserves DB cluster comparison in the VSCode legacy loader', async () => {
    process.env.REACT_APP_IS_VSCODE = 'true';
    await setBaselineData(input);
    expect(message.warning).not.toHaveBeenCalled();
    expect(setBaseline).toHaveBeenCalledWith(input);
    expect(isInClusterCompare()).toBe(true);
    expect(clusterView.version).toBe('modern');
});
it('keeps rank-level Timeline baseline available in modern mode', async () => {
    (setBaseline as jest.Mock).mockResolvedValue({ isCluster: false, rankId: 'result-rank', cardId: '1', dbPath: '/rank.db' });
    await setBaselineData({ ...input, fileType: 'RANK', rankId: 'original-rank' });
    expect(setBaseline).toHaveBeenCalled(); expect(store.sessionStore.activeSession.compareSet.baseline.rankId).toBe('original-rank');
});
it('preserves the upstream card validation and releases the version lock for an invalid rank baseline', async () => {
    (setBaseline as jest.Mock).mockResolvedValue({ isCluster: false, rankId: '', cardId: ' ' });
    await setBaselineData({ ...input, fileType: 'RANK', rankId: 'original-rank' });
    expect(message.warning).toHaveBeenCalledWith('FileCheckFailedDescribe');
    expect(store.sessionStore.activeSession.compareSet.baseline.projectName).toBe('');
    expect(clusterView.comparisonPending).toBe(false);
});
it.each([
    { isFtrace: true, comparisonType: ProjectType.DB },
    { isFtrace: false, comparisonType: ProjectType.DB_FTRACE },
])('retains the Ftrace/profiling comparison guard with baseline isFtrace=$isFtrace', async ({ isFtrace, comparisonType }) => {
    (setBaseline as jest.Mock).mockResolvedValue({ isCluster: false, rankId: 'baseline-rank', cardId: '1', isFtrace });
    await setBaselineData({ ...input, fileType: 'RANK', rankId: 'original-rank' });
    expect(store.sessionStore.activeSession.compareSet.baseline.isFtrace).toBe(isFtrace);
    store.sessionStore.activeSession.activeDataSource.children[0].children = [{
        type: 'RANK', name: 'rank', path: '/current/rank', rankId: 'comparison-rank', projectType: comparisonType, children: [],
    }];
    setCompareData({ projectName: 'current', fileType: 'RANK', filePath: '/current/rank', rankId: 'comparison-rank' });
    expect(message.warning).toHaveBeenCalledWith('Comparison is not allowed for different data types.');
    expect(store.sessionStore.activeSession.compareSet.comparison.rankId).toBe('');
});
it('blocks cluster comparison selection in modern mode', () => {
    setCompareData({ ...input, projectName: 'current' });
    expect(message.warning).toHaveBeenCalledWith('clusterView.compareUnavailable');
    expect(store.sessionStore.activeSession.compareSet.comparison).toEqual({});
});
it('releases the version lock on backend failure', async () => {
    clusterView.select('legacy'); (setBaseline as jest.Mock).mockRejectedValue(new Error('offline'));
    await expect(setBaselineData(input)).rejects.toThrow('offline'); expect(clusterView.comparisonPending).toBe(false);
});

it('preserves cluster comparison for text data despite a modern preference', async () => {
    store.sessionStore.activeSession.activeDataSource.children[0].projectType = ProjectType.TEXT_CLUSTER;
    await setBaselineData(input);
    expect(setBaseline).toHaveBeenCalledWith(input);
    expect(isInClusterCompare()).toBe(true);
    expect(clusterView.version).toBe('modern');
});
