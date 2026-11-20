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
import React from 'react';
import { act, fireEvent } from '@testing-library/react';
import { renderWithProviders } from '../../../../testing/render';
import ToolBox from './Index';
import type { Session } from '../../entity/session';
import { ProjectType } from '../../utils/enum';
import { clusterView } from '../../cluster/viewVersion';

const initialEnv = process.env;
beforeEach(() => { process.env = { ...initialEnv, REACT_APP_ENABLE_MODERN_CLUSTER: 'true', REACT_APP_IS_VSCODE: 'false' }; });
afterEach(() => { process.env = initialEnv; });

jest.mock('./SwitchTheme', () => function Theme() { return <button>Theme</button>; });
jest.mock('./SwitchLanguage', () => function Language() { return <button>Language</button>; });
jest.mock('./Version', () => function Help() { return <button>Help</button>; });
jest.mock('@/utils/Compare', () => ({ isInClusterCompare: () => false }));
const sessionFor = (projectType: ProjectType): Session => ({
    activeDataSource: { children: [{ type: 'CLUSTER', path: '/cluster', projectType }] },
    clusterPageInfo: { selectedClusterPath: '/cluster' },
} as unknown as Session);
it.each(['Summary', 'Communication'])('hides the version switch on %s when disabled or running in VSCode', activeModule => {
    delete process.env.REACT_APP_ENABLE_MODERN_CLUSTER;
    const view = renderWithProviders(<ToolBox activeModule={activeModule} session={sessionFor(ProjectType.DB_CLUSTER)} />);
    expect(view.getAllByRole('button')).toHaveLength(3);
    process.env.REACT_APP_ENABLE_MODERN_CLUSTER = 'true'; process.env.REACT_APP_IS_VSCODE = 'true';
    view.rerender(<ToolBox activeModule={activeModule} session={sessionFor(ProjectType.DB_CLUSTER)} />);
    expect(view.getAllByRole('button')).toHaveLength(3);
    expect(view.queryByRole('button', { name: 'clusterView.legacy' })).not.toBeInTheDocument();
    expect(view.queryByRole('button', { name: 'clusterView.modern' })).not.toBeInTheDocument();
});
it('shows the switch only for DB cluster tabs and removes it on text data', () => {
    clusterView.select('modern');
    const view = renderWithProviders(<ToolBox activeModule="Communication" session={sessionFor(ProjectType.DB_CLUSTER)} />);
    expect(view.getByRole('button', { name: 'clusterView.legacy' })).toBeInTheDocument();
    view.rerender(<ToolBox activeModule="Communication" session={sessionFor(ProjectType.TEXT_CLUSTER)} />);
    expect(view.queryByRole('button', { name: 'clusterView.legacy' })).not.toBeInTheDocument();
    expect(view.getAllByRole('button')).toHaveLength(3);
    view.rerender(<ToolBox activeModule="Timeline" session={sessionFor(ProjectType.DB_CLUSTER)} />);
    expect(view.queryByRole('button', { name: 'clusterView.legacy' })).not.toBeInTheDocument();
    view.rerender(<ToolBox activeModule="Summary" session={sessionFor(ProjectType.DB_CLUSTER)} />);
    expect(view.getByRole('button', { name: 'clusterView.legacy' })).toBeInTheDocument();
});

it('switches immediately, ignores rapid clicks across cluster tabs, and accepts a fresh click at 500 ms', () => {
    jest.useFakeTimers();
    try {
        clusterView.select('modern');
        const session = sessionFor(ProjectType.DB_CLUSTER);
        const view = renderWithProviders(<ToolBox activeModule="Communication" session={session} />);
        const button = view.getByRole('button', { name: 'clusterView.legacy' });
        fireEvent.click(button);
        expect(clusterView.version).toBe('legacy');
        fireEvent.click(button);
        expect(clusterView.version).toBe('legacy');
        view.rerender(<ToolBox activeModule="Summary" session={session} />);
        act(() => { jest.advanceTimersByTime(499); });
        fireEvent.click(view.getByRole('button', { name: 'clusterView.modern' }));
        expect(clusterView.version).toBe('legacy');
        act(() => { jest.advanceTimersByTime(1); });
        expect(clusterView.version).toBe('legacy');
        fireEvent.click(view.getByRole('button', { name: 'clusterView.modern' }));
        expect(clusterView.version).toBe('modern');
    } finally {
        jest.useRealTimers();
    }
});
