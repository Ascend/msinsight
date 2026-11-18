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
import { act, fireEvent, render } from '@testing-library/react';
import { runInAction } from 'mobx';
import { createInstance } from 'i18next';
import { I18nextProvider, initReactI18next } from 'react-i18next';
import en from '../../../lib/src/i18n/leaks/en.json';
import zh from '../../../lib/src/i18n/leaks/zh.json';
import MemoryStack from '../components/MemoryStack';
import { createSession } from '../application/session';
import { getSnapshotEvent } from '../api/requests';

jest.mock('../api/requests', () => ({ getSnapshotEvent: jest.fn() }));
jest.mock('@insight/lib/components', () => ({
    CollapsiblePanel: ({ children }: { children: React.ReactNode }) => children,
    Select: () => null,
    Tabs: ({ items, activeKey, onChange, onEdit }: {
        items: Array<{ key: string; label: React.ReactNode; children: React.ReactNode }>;
        activeKey: string;
        onChange: (key: string) => void;
        onEdit: (key: string, action: string) => void;
    }) => <>
        <div role="tablist">{items.map(item => <span key={item.key}>
            <button role="tab" aria-label={`Tab ${item.key}`} aria-selected={activeKey === item.key} onClick={(): void => onChange(item.key)}>{item.label}</button>
            <button onClick={(): void => onEdit(item.key, 'remove')}>Close {item.key}</button>
        </span>)}</div>
        {items.find(item => item.key === activeKey)?.children}
    </>,
}));
jest.mock('../common/components/Common', () => ({ Label: () => null }));
jest.mock('../common/components/MemoryDataZoom', () => () => null);
jest.mock('../components/MemoryStateDiagram', () => ({ MemoryStateDiagram: () => null }));
jest.mock('../components/PotentialLeakStats', () => () => null);
jest.mock('../components/MemSnapshotSliceOverview', () => () => null);
jest.mock('../loaders/data', () => ({
    getBarNewData: jest.fn(),
    getBlockTableData: jest.fn(),
    getEventTableData: jest.fn(),
    getPotentialLeakStats: jest.fn(),
    preloadSnapshotSliceOverviews: jest.fn(),
}));
jest.mock('../common/leaksWorker/blockWorker/worker', () => ({
    getBlockRenderer: () => ({ removeMemoryBlockCaches: jest.fn(), setBlockGraphGlobalMaxSize: jest.fn() }),
}));
jest.mock('../rendering/stateWorker/worker', () => ({ getStateRenderer: () => ({ selectItem: jest.fn() }) }));
jest.mock('../common/shared/hooks/useLifecycleRange', () => ({
    useLifecycleRange: () => ({
        selectedRange: undefined,
        setSelectedRange: jest.fn(),
        selectedRangeRef: { current: undefined },
        zoomRangeRef: { current: { min: 0, max: 100 } },
        debouncedRangeRequestRef: { current: null },
        debouncedCommitRangeRef: { current: null },
        rangeRequestSeqRef: { current: 0 },
        commitRangeSideEffects: jest.fn(),
        requestRangeData: jest.fn(),
        selectedZoomChange: jest.fn(),
        syncDataZoomRange: jest.fn(),
    }),
}));
jest.mock('../common/components/leaks/MemoryBlockDiagram', () => ({
    MemoryBlockDiagram: ({ onSelectTimestamp, timeMarkers = [], onRemoveTimeMarker, onUpdateTimeMarker }: {
        onSelectTimestamp: (value: number) => void;
        timeMarkers?: Array<{ id: number; label: string; color?: string; hidden?: boolean }>;
        onRemoveTimeMarker: (id: number) => void;
        onUpdateTimeMarker: (id: number, updates: { label?: string; color?: string; hidden?: boolean }) => void;
    }) => <>
        <button onClick={(): void => onSelectTimestamp(135.8)}>Select event</button>
        {timeMarkers.map(marker => <span key={marker.id}>
            <span data-testid="marker-name" data-color={marker.color} data-hidden={marker.hidden === true}>{marker.label}</span>
            <button onClick={(): void => onRemoveTimeMarker(marker.id)}>Remove {marker.id}</button>
            <button onClick={(): void => onUpdateTimeMarker(marker.id, { label: 'Forward end' })}>Rename {marker.id}</button>
            <button onClick={(): void => onUpdateTimeMarker(marker.id, { color: '#2FA87C', hidden: !marker.hidden })}>Appearance {marker.id}</button>
        </span>)}
    </>,
}));
jest.mock('../components/FrameTreePanel', () => ({
    FrameTreePanel: (props: unknown) => <div data-testid="selection-output">{JSON.stringify(props)}</div>,
}));

const translations = createInstance();
const showStack = (session: ReturnType<typeof createSession>): ReturnType<typeof render> =>
    render(<I18nextProvider i18n={translations}><MemoryStack session={session} /></I18nextProvider>);

describe('timeline selection context isolation', () => {
    beforeAll(async () => {
        await translations.use(initReactI18next).init({
            lng: 'enUS', fallbackLng: 'enUS', resources: { enUS: en, zhCN: zh }, interpolation: { escapeValue: false },
        });
    });
    beforeEach(async () => { await translations.changeLanguage('enUS'); });
    afterEach(() => jest.useRealTimers());
    it('retains custom names and marker appearance through language changes and pending event responses', async () => {
        jest.useFakeTimers();
        let finish: (value: unknown) => void = () => undefined;
        (getSnapshotEvent as jest.Mock).mockResolvedValueOnce({ events: [{ id: 130 }] })
            .mockImplementationOnce(() => new Promise(resolve => { finish = resolve; }));
        const session = createSession();
        runInAction(() => { session.deviceId = '0'; session.eventType = 'BLOCK'; session.fileHash = 'language'; });
        const view = showStack(session);
        const names = (): Array<string | null> => view.getAllByTestId('marker-name').map(element => element.textContent);
        fireEvent.click(view.getByRole('button', { name: 'Select event' }));
        await act(async () => { jest.advanceTimersByTime(150); });
        expect(names()).toEqual(['Event 130']);
        await act(async () => { await translations.changeLanguage('zhCN'); });
        expect(names()).toEqual(['\u4e8b\u4ef6 130']);
        expect(view.getByRole('tab', { name: 'Tab 1' }).textContent).toBe('\u4e8b\u4ef6 130');
        fireEvent.click(view.getByRole('button', { name: 'Rename 1' }));
        fireEvent.click(view.getByRole('button', { name: 'Select event' }));
        await act(async () => { jest.advanceTimersByTime(150); });
        expect(names()).toEqual(['Forward end', '\u4e8b\u4ef6 135']);
        fireEvent.click(view.getByRole('button', { name: 'Appearance 2' }));
        await act(async () => { await translations.changeLanguage('enUS'); });
        expect(names()).toEqual(['Forward end', 'Event 135']);
        await act(async () => { finish({ events: [{ id: 132 }] }); });
        expect(names()).toEqual(['Forward end', 'Event 132']);
        await act(async () => { await translations.changeLanguage('zhCN'); });
        expect(names()).toEqual(['Forward end', '\u4e8b\u4ef6 132']);
        expect(view.getByRole('tab', { name: 'Tab 2' }).textContent).toBe('\u4e8b\u4ef6 132');
        expect(view.getAllByTestId('marker-name')[1].getAttribute('data-color')).toBe('#2FA87C');
        expect(view.getAllByTestId('marker-name')[1].getAttribute('data-hidden')).toBe('true');
        expect(view.getAllByRole('tab')).toHaveLength(2);
        fireEvent.click(view.getByRole('button', { name: 'Appearance 2' }));
        expect(view.getAllByTestId('marker-name')[1].getAttribute('data-hidden')).toBe('false');
        expect(getSnapshotEvent).toHaveBeenCalledTimes(2);
    });
    it('retains rapid selections resolved out of order and does not restore a deleted pending marker', async () => {
        jest.useFakeTimers();
        const pending: Array<(value: unknown) => void> = [];
        (getSnapshotEvent as jest.Mock).mockImplementation(() => new Promise(resolve => pending.push(resolve)));
        const session = createSession();
        runInAction(() => { session.deviceId = '0'; session.eventType = 'BLOCK'; session.fileHash = 'multiple'; });
        const view = showStack(session);
        for (let index = 0; index < 3; index++) {
            fireEvent.click(view.getByRole('button', { name: 'Select event' }));
            await act(async () => { jest.advanceTimersByTime(150); });
        }
        expect(pending).toHaveLength(3);
        fireEvent.click(view.getByRole('button', { name: 'Remove 3' }));
        fireEvent.click(view.getByRole('button', { name: 'Rename 1' }));
        await act(async () => {
            pending[1]({ events: [{ id: 132 }] });
            pending[0]({ events: [{ id: 130 }] });
            pending[2]({ events: [{ id: 134 }] });
        });
        expect(view.getAllByRole('tab')).toHaveLength(2);
        expect(view.getAllByTestId('marker-name').map(element => element.textContent)).toEqual(['Forward end', 'Event 132']);
        expect(view.getAllByTestId('selection-output')).toHaveLength(1);
        expect(JSON.parse(view.getByTestId('selection-output').textContent ?? '{}').context.eventId).toBe(132);
        fireEvent.click(view.getByRole('tab', { name: 'Tab 1' }));
        expect(JSON.parse(view.getByTestId('selection-output').textContent ?? '{}').context.eventId).toBe(130);
        fireEvent.click(view.getByRole('button', { name: 'Close 1' }));
        expect(view.getAllByRole('tab')).toHaveLength(1);
        expect(JSON.parse(view.getByTestId('selection-output').textContent ?? '{}').context.eventId).toBe(132);
    });
    it.each([
        ['device', 'success'], ['device', 'failure'], ['slice', 'success'],
        ['slice', 'failure'], ['dataset', 'success'], ['dataset', 'failure'],
    ])('discards old %s context %s after selecting a new event', async (change, outcome) => {
        jest.useFakeTimers();
        let resolveOld: (value: unknown) => void = () => undefined;
        let rejectOld: (error: Error) => void = () => undefined;
        const old = new Promise((resolve, reject) => { resolveOld = resolve; rejectOld = reject; });
        (getSnapshotEvent as jest.Mock).mockReturnValueOnce(old)
            .mockResolvedValue({ total: 1, events: [{ id: 132 }] });
        const session = createSession();
        runInAction(() => {
            session.deviceIds = { 0: ['BLOCK'], 1: ['BLOCK'] };
            session.deviceId = '0';
            session.eventType = 'BLOCK';
            session.fileHash = 'dataset-a';
            session.selectedSliceIndex = 0;
        });
        const view = showStack(session);
        fireEvent.click(view.getByRole('button', { name: 'Select event' }));
        await act(async () => { jest.advanceTimersByTime(150); });
        expect(getSnapshotEvent).toHaveBeenCalledTimes(1);
        act(() => runInAction(() => {
            if (change === 'device') session.deviceId = '1';
            else if (change === 'slice') session.selectedSliceIndex = 1;
            else session.fileHash = 'dataset-b';
        }));
        expect(JSON.parse(view.getByTestId('selection-output').textContent ?? '{}').context).toBeNull();
        fireEvent.click(view.getByRole('button', { name: 'Select event' }));
        await act(async () => { jest.advanceTimersByTime(150); });
        const current = view.getByTestId('selection-output').textContent;
        expect(JSON.parse(current ?? '{}').context.eventId).toBe(132);
        await act(async () => {
            if (outcome === 'success') resolveOld({ total: 1, events: [{ id: 130 }] });
            else rejectOld(new Error('stale error'));
        });
        expect(view.getByTestId('selection-output').textContent).toBe(current);
        view.unmount();
    });
});
