/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React from 'react';
import { act, fireEvent, render } from '@testing-library/react';
import i18n from '@insight/lib/i18n';
import type { NumaOverview } from '@/entities/numa/types';
import { NumaOverviewPage } from '@/features/overview/NumaOverviewPage';
import { NumaStore } from '@/features/overview/model/NumaStore';
import { deferred } from '@/testUtils/deferred';

jest.mock('@insight/lib/components', () => ({
    MITooltipHelp: (): React.ReactElement => <span>?</span>,
}), { virtual: true });

const overview: NumaOverview = {
    range: { startTime: 0, endTime: 10 },
    totalMetrics: [],
    sockets: [{ id: 0, name: 'Socket 0', metrics: [], numas: [] }],
    connections: [],
};

const socketTraffic = (hasValue: boolean): NumaOverview => ({
    ...overview,
    sockets: [...overview.sockets, { id: 1, name: 'Socket 1', metrics: [], numas: [] }],
    connections: [{
        id: 'socket-link-0-1',
        type: 'socket',
        source: 'socket-0',
        target: 'socket-1',
        label: 'Socket 0 <-> Socket 1',
        description: '',
        metrics: [
            { key: 'sourceToTarget', label: '', description: '', unit: 'GB', value: Number(hasValue), hasValue },
            { key: 'targetToSource', label: '', description: '', unit: 'GB', value: 0, hasValue: false },
        ],
    }],
});

async function loadedStore(data: NumaOverview): Promise<NumaStore> {
    const store = new NumaStore(async () => data);
    await store.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'db-path' });
    return store;
}

describe('NumaOverviewPage', () => {
    beforeEach(async () => i18n.changeLanguage('enUS'));

    it('renders loading, error, and loaded states from the Store', async () => {
        const pending = deferred<NumaOverview>();
        const loadingStore = new NumaStore(() => pending.promise);
        void loadingStore.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'db-path' });
        const rendered = render(<NumaOverviewPage store={loadingStore} />);
        expect(rendered.getByText('Building NUMA topology from platform metrics...')).toBeTruthy();

        await act(async () => {
            pending.resolve(overview);
            await pending.promise;
        });
        expect(rendered.getByText(/1 SOCKETS/)).toBeTruthy();

        const errorStore = new NumaStore(async () => { throw new Error('Unable to read platform metrics'); });
        await errorStore.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'db-path' });
        rendered.rerender(<NumaOverviewPage store={errorStore} />);
        expect(rendered.getByText('Unable to read platform metrics')).toBeTruthy();
    });

    it.each([
        [false, true],
        [true, false],
    ])('shows the protocol notice for available traffic=%s only when needed', async (hasValue, visible) => {
        const { queryByText } = render(<NumaOverviewPage store={await loadedStore(socketTraffic(hasValue))} />);
        const notice = queryByText('Protocol Adapter wasn\'t enabled or not supported (A2)');
        expect(notice !== null).toBe(visible);
    });

    it('renders NUMA-owned text in the selected language', async () => {
        await i18n.changeLanguage('zhCN');
        const store = await loadedStore(overview);
        store.setLocale('zhCN');

        const { getByText } = render(<NumaOverviewPage store={store} />);
        expect(getByText('NUMA 高层架构图')).toBeTruthy();
        expect(getByText('详细信息')).toBeTruthy();
    });

    it('supports zoom and connection metric toolbar actions', async () => {
        const store = await loadedStore(socketTraffic(true));
        const { container, getByRole, getByText, getByTitle } = render(<NumaOverviewPage store={store} />);

        fireEvent.click(getByTitle('Zoom in'));
        expect(getByText('110%')).toBeTruthy();
        fireEvent.click(getByTitle('Reset zoom'));
        expect(getByText('100%')).toBeTruthy();

        const diagramScroll = container.querySelector('.diagram-scroll');
        if (!diagramScroll) throw new Error('Expected diagram scroll container');
        fireEvent.wheel(diagramScroll, { ctrlKey: true, deltaY: 1 });
        expect(getByText('90%')).toBeTruthy();

        const toggle = getByRole('button', { name: 'Show connection metrics' });
        expect(container.querySelector('.connection-metric-label')).toBeTruthy();
        fireEvent.click(toggle);
        expect(toggle.getAttribute('aria-pressed')).toBe('false');
        expect(container.querySelector('.connection-metric-label')).toBeNull();
        expect(container.querySelector('.connection-line')).toBeTruthy();
    });
});
