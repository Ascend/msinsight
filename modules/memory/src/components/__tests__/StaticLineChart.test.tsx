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
import React from 'react';
import { act } from '@testing-library/react';
import * as testingLibrary from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { runInAction } from 'mobx';
import { renderWithProviders } from '../../../../testing/render';
import { deferred } from '../../../../testing/deferred';
import { customConsole } from '../../../../lib/src/utils/Console';
import StaticLineChart from '../StaticLineChart';
import { Session } from '../../entity/session';
import { MemorySession } from '../../entity/memorySession';
import type { Graph, StaticOperatorCurve } from '../../entity/memory';

const { screen, waitFor } = testingLibrary;

// Canvas rendering has separate tests; keep the real selector, MobX state and request adapter.
jest.mock('../LineChart', () => ({
    LineChart: ({ graph, onSelectionChanged }: {
        graph: Graph;
        onSelectionChanged: (start: number, end: number) => void;
    }) => (
        <div>
            <output aria-label="Static memory curve">{graph.columns?.join(',')}</output>
            <button onClick={() => onSelectionChanged(0, 1)}>Select interval</button>
            <button onClick={() => onSelectionChanged(1, 0)}>Select reversed interval</button>
        </div>
    ),
}));

const curve: StaticOperatorCurve = {
    legends: ['allocated', 'reserved'],
    lines: [['100', '200', '300'], ['150', '250', '350']],
};
const resources = {
    enUS: { memory: { allocated: 'Allocated', reserved: 'Reserved' } },
};

function createFixture(): { session: Session; memorySession: MemorySession; isDark: boolean } {
    const session = new Session();
    const memorySession = new MemorySession();
    runInAction(() => {
        memorySession.memoryGraphId = 'graph-1';
        memorySession.memoryGraphIdList = ['graph-1', 'graph-2'];
        memorySession.rankCondition = {
            value: 0,
            options: ['rank-1', 'rank-2'].map((rankId, index) => ({
                rankInfo: { rankId, rankName: rankId, clusterId: '', host: '', deviceId: '' },
                dbPath: `/test/${rankId}`,
                index,
            })),
        };
    });
    return { session, memorySession, isDark: false };
}

describe('StaticLineChart request and observable state integration', () => {
    let props: ReturnType<typeof createFixture>;
    let pending: ReturnType<typeof deferred<StaticOperatorCurve>>;
    let request: jest.Mock;
    let originalRequest: PropertyDescriptor | undefined;

    beforeEach(() => {
        props = createFixture();
        pending = deferred<StaticOperatorCurve>();
        request = jest.fn(({ command }: { command: string }) => {
            if (command !== 'Memory/view/staticOpMemoryGraph') throw new Error(`Unexpected command: ${command}`);
            return pending.promise;
        });
        originalRequest = Object.getOwnPropertyDescriptor(window, 'request');
        Object.defineProperty(window, 'request', { configurable: true, writable: true, value: request });
    });

    afterEach(() => {
        if (originalRequest) Object.defineProperty(window, 'request', originalRequest);
        else Reflect.deleteProperty(window, 'request');
    });

    async function respond(data: StaticOperatorCurve = curve): Promise<void> {
        await act(async () => {
            pending.resolve(data);
            await pending.promise;
        });
    }

    it('shows loading until the response arrives and renders translated curve data', async () => {
        const view = renderWithProviders(<StaticLineChart {...props} />, { resources });
        expect(request).toHaveBeenCalledWith({
            command: 'Memory/view/staticOpMemoryGraph',
            params: { rankId: 'rank-1', dbPath: '/test/rank-1', graphId: 'graph-1', isCompare: false },
        });
        await waitFor(() => expect(view.container.querySelector('.ant-spin-spinning')).toBeInTheDocument());
        expect(screen.queryByLabelText('Static memory curve')).not.toBeInTheDocument();

        await respond();

        expect(screen.getByLabelText('Static memory curve')).toHaveTextContent('Allocated,Reserved');
        await waitFor(() => expect(view.container.querySelector('.ant-spin-spinning')).not.toBeInTheDocument());
    });

    it('changes the graph through the real selector and resets table state', async () => {
        const user = userEvent.setup();
        renderWithProviders(<StaticLineChart {...props} />, { resources });
        await respond();
        runInAction(() => {
            props.memorySession.current = 4;
            props.memorySession.pageSize = 50;
            props.memorySession.searchEventOperatorName = 'old operator';
            props.memorySession.staticSelectedRange = { startTs: 1, endTs: 2 };
        });
        await user.click(screen.getByRole('combobox'));
        await user.click(await screen.findByTitle('graph-2'));

        expect(props.memorySession.memoryGraphId).toBe('graph-2');
        expect(props.memorySession.current).toBe(1);
        expect(props.memorySession.pageSize).toBe(10);
        expect(props.memorySession.searchEventOperatorName).toBe('');
        expect(props.memorySession.staticSelectedRange).toBeUndefined();
        await waitFor(() => expect(request).toHaveBeenLastCalledWith({
            command: 'Memory/view/staticOpMemoryGraph',
            params: { rankId: 'rank-1', dbPath: '/test/rank-1', graphId: 'graph-2', isCompare: false },
        }));
    });

    it('reacts to a rank change without manually rerendering the component', async () => {
        renderWithProviders(<StaticLineChart {...props} />, { resources });
        await respond();
        await act(async () => runInAction(() => { props.memorySession.rankCondition.value = 1; }));
        expect(request).toHaveBeenCalledTimes(2);
        expect(request).toHaveBeenLastCalledWith({
            command: 'Memory/view/staticOpMemoryGraph',
            params: { rankId: 'rank-2', dbPath: '/test/rank-2', graphId: 'graph-1', isCompare: false },
        });
    });

    it('reacts to comparison mode changes', async () => {
        renderWithProviders(<StaticLineChart {...props} />, { resources });
        await respond();
        await act(async () => runInAction(() => { props.session.compareRank.isCompare = true; }));
        expect(request).toHaveBeenLastCalledWith({
            command: 'Memory/view/staticOpMemoryGraph',
            params: { rankId: 'rank-1', dbPath: '/test/rank-1', graphId: 'graph-1', isCompare: true },
        });
    });

    it.each(['rank', 'graph'])('does not request data when the %s is missing', (missing) => {
        runInAction(() => {
            if (missing === 'rank') props.memorySession.rankCondition.value = undefined;
            else props.memorySession.memoryGraphId = '';
        });
        renderWithProviders(<StaticLineChart {...props} />, { resources });
        expect(request).not.toHaveBeenCalled();
        expect(screen.queryByLabelText('Static memory curve')).not.toBeInTheDocument();
    });

    it('clears the displayed curve when the selected rank is removed', async () => {
        renderWithProviders(<StaticLineChart {...props} />, { resources });
        await respond();
        expect(screen.getByLabelText('Static memory curve')).toBeInTheDocument();
        await act(async () => runInAction(() => { props.memorySession.rankCondition.value = undefined; }));
        expect(screen.queryByLabelText('Static memory curve')).not.toBeInTheDocument();
        expect(request).toHaveBeenCalledTimes(1);
    });

    it('maps a selected curve interval to timestamps and rejects reversed intervals', async () => {
        const user = userEvent.setup();
        renderWithProviders(<StaticLineChart {...props} />, { resources });
        await respond();
        await user.click(screen.getByRole('button', { name: 'Select interval' }));
        expect(props.memorySession.staticSelectedRange).toEqual({ startTs: 100, endTs: 150 });
        await user.click(screen.getByRole('button', { name: 'Select reversed interval' }));
        expect(props.memorySession.staticSelectedRange).toBeUndefined();
    });

    it('does not select an interval when the curve has only one timestamp', async () => {
        const user = userEvent.setup();
        renderWithProviders(<StaticLineChart {...props} />, { resources });
        await respond({ ...curve, lines: [curve.lines[0]] });
        await user.click(screen.getByRole('button', { name: 'Select interval' }));
        expect(props.memorySession.staticSelectedRange).toBeUndefined();
    });

    it('ends loading and reports a rejected request', async () => {
        const error = new Error('query failed');
        const errorSpy = jest.spyOn(customConsole, 'error');
        const view = renderWithProviders(<StaticLineChart {...props} />, { resources });
        await act(async () => {
            pending.reject(error);
            await pending.promise.catch(() => {});
        });
        await waitFor(() => expect(view.container.querySelector('.ant-spin-spinning')).not.toBeInTheDocument());
        expect(errorSpy).toHaveBeenCalledWith(error);
        expect(screen.queryByLabelText('Static memory curve')).not.toBeInTheDocument();
    });
});
