/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import { ThemeProvider } from '@emotion/react';
import { themeInstance } from '@insight/lib/theme';
import { act, cleanup, fireEvent, render, screen, within } from '@testing-library/react';
import '@testing-library/jest-dom';
import { SessionBatchExportDialog } from '../../components/SessionBatchExportDialog';
import { exportSessionBatch, fetchSessionExportStatus, querySessionExports } from '../../api';
import { shouldPersistSessionExport } from '../../env';
import { downloadSessionExport } from '../../sessionExportDownload';
import type { SessionBatchExportDocument, SessionExportStatus } from '../../types';

jest.mock('../../api', () => ({
    querySessionExports: jest.fn(),
    fetchSessionExportStatus: jest.fn(),
    exportSessionBatch: jest.fn(),
}));
jest.mock('../../env', () => ({ shouldPersistSessionExport: jest.fn() }));
jest.mock('../../sessionExportDownload', () => ({ downloadSessionExport: jest.fn() }));

const queryMock = querySessionExports as jest.Mock;
const statusMock = fetchSessionExportStatus as jest.Mock;
const batchMock = exportSessionBatch as jest.Mock;
const persistMock = shouldPersistSessionExport as jest.Mock;
const downloadMock = downloadSessionExport as jest.Mock;
const agents = [{ name: 'Owner A', available: true }, { name: 'Owner B', available: true }, { name: 'Unspecified availability' }, { name: 'Unavailable', available: false }];
const filters = { agentIds: ['Owner A', 'Owner B'], keyword: '', createdFrom: '2026-09-28T06:00:00.000Z', createdTo: '2026-10-28T06:00:00.000Z' };
const matches = Array.from({ length: 12 }, (_, index) => ({
    key: `opaque-${index}`, agentId: index % 2 ? 'Owner B' : 'Owner A',
    sessionId: `session-${index}`, title: `Chat ${index}`, updatedAt: '2026-10-27T06:00:00.000Z',
    primaryAgentId: 'not-an-owner',
}));
const ready = (overrides: Partial<SessionExportStatus> = {}): SessionExportStatus => ({
    jobId: 'job-1', phase: 'ready', processed: 12, total: 12, items: matches, issues: [], filters, ...overrides,
});
const artifact = (savedPath?: string): SessionBatchExportDocument => ({
    format: 'msinsight.agent-sessions.v1', filename: 'batch.json', exportedAt: filters.createdTo, filters, issues: [], sessions: [], ...(savedPath ? { savedPath } : {}),
});
const deferred = <T,>(): { promise: Promise<T>; resolve: (value: T) => void } => {
    let resolve!: (value: T) => void;
    const promise = new Promise<T>((done) => { resolve = done; });
    return { promise, resolve };
};
const show = () => {
    const onOpenChange = jest.fn();
    return { ...render(<SessionBatchExportDialog open agents={agents} onOpenChange={onOpenChange} />), onOpenChange };
};
const query = async () => {
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: 'Find conversations' })); });
};
const tick = async () => { await act(async () => { jest.advanceTimersByTime(750); }); };
const agentCheckbox = (name: string) => screen.getByRole('checkbox', { name });
const toggleAgent = (name: string) => fireEvent.click(agentCheckbox(name));
const titleInput = () => screen.getByRole('textbox', { name: 'Session title' });
const choosePreset = (name: string) => fireEvent.click(within(screen.getByRole('region', { name: 'Creation time' })).getByRole('button', { name }));
const chooseCustom = () => choosePreset('Custom');
const setRange = (from: string, to: string) => {
    fireEvent.change(screen.getByLabelText('Creation start'), { target: { value: from } });
    fireEvent.change(screen.getByLabelText('Creation end'), { target: { value: to } });
};

beforeEach(() => {
    jest.useFakeTimers();
    jest.resetAllMocks();
    queryMock.mockResolvedValue({ jobId: 'job-1' });
    batchMock.mockResolvedValue({ jobId: 'job-1' });
    statusMock.mockResolvedValue(ready());
    persistMock.mockReturnValue(false);
});
afterEach(() => { cleanup(); jest.useRealTimers(); });

test('keeps the title label readable in the dark theme', () => {
    const theme = themeInstance.getTheme().dark;
    render(<ThemeProvider theme={theme}><SessionBatchExportDialog open agents={agents} onOpenChange={jest.fn()} /></ThemeProvider>);
    expect(screen.getByText('Session title', { selector: 'label' })).toHaveStyle({ color: theme.textColorPrimary });
});

test('defaults to all available agents and all creation time, with export guidance only in the header tooltip', async () => {
    const { container } = show();
    expect(screen.getByRole('dialog', { name: /Export historical sessions/ })).toBeInTheDocument();
    expect(screen.getByRole('region', { name: 'Agents' })).toBeInTheDocument();
    expect(screen.getByRole('region', { name: 'Creation time' })).toBeInTheDocument();
    expect(screen.queryByRole('region', { name: 'Updated time' })).not.toBeInTheDocument();
    expect(screen.getByRole('region', { name: 'Session title' })).toBeInTheDocument();
    expect(screen.getByText('3 of 3 selected')).toBeInTheDocument();
    const createdRegion = screen.getByRole('region', { name: 'Creation time' });
    expect(createdRegion.querySelector('.active')).toHaveTextContent('All');
    expect(screen.getByRole('region', { name: 'Agents' }).querySelector('.help-icon')).toBeNull();
    expect(container.querySelectorAll('.help-icon').length).toBe(3);
    expect(screen.queryByText(/Filter and export history/)).not.toBeInTheDocument();
    for (const name of ['Owner A', 'Owner B', 'Unspecified availability']) expect(agentCheckbox(name)).toBeChecked();
    expect(screen.queryByRole('checkbox', { name: 'Unavailable' })).not.toBeInTheDocument();
    await query();
    expect(queryMock).toHaveBeenLastCalledWith({ agentIds: ['Owner A', 'Owner B', 'Unspecified availability'], allTime: true, keyword: '' }, expect.any(AbortSignal));
    expect(container.querySelector('details')).toBeNull();
    fireEvent.focus(screen.getByRole('button', { name: 'About session export' }));
    await tick();
    expect(screen.getByRole('tooltip')).toHaveTextContent('Filter and export history without switching or interrupting your current chat.');
    expect(screen.getByRole('tooltip')).toHaveTextContent(`Creation time range (inclusive, UTC): [${filters.createdFrom}, ${filters.createdTo}]`);
});

test('shows a themed brand logo or a decorative generic icon without changing the owner ID', async () => {
    render(<SessionBatchExportDialog open agents={[{ name: 'Codex' }, { name: 'Custom agent' }]} onOpenChange={jest.fn()} />);
    const branded = agentCheckbox('Codex').closest('label');
    const custom = agentCheckbox('Custom agent').closest('label');
    expect(branded?.querySelector('img[data-agent-icon="themed"]')).toHaveAttribute('alt', '');
    expect(custom?.querySelector('.agent-logo .anticon-robot')).toBeInTheDocument();
    fireEvent.click(screen.getByText('Custom agent'));
    expect(screen.getByText('1 of 2 selected')).toBeInTheDocument();
    await query();
    expect(queryMock.mock.calls[0][0].agentIds).toEqual(['Codex']);
});

test('multi-selects actual owner descriptors and passes title queries unchanged to the backend', async () => {
    show();
    toggleAgent('Unspecified availability');
    choosePreset('Last week');
    fireEvent.change(titleInput(), { target: { value: 'Ds .* KEYword' } });
    await query();
    expect(queryMock).toHaveBeenLastCalledWith({ agentIds: ['Owner A', 'Owner B'], createdDays: 7, keyword: 'Ds .* KEYword' }, expect.any(AbortSignal));
    fireEvent.click(screen.getByRole('button', { name: 'All agents' }));
    await query();
    expect(queryMock.mock.calls[1][0].agentIds).toEqual(['Owner A', 'Owner B', 'Unspecified availability']);
});

test('explicit clearing blocks submission instead of treating an empty selection as all', async () => {
    show();
    fireEvent.click(screen.getByRole('button', { name: 'Clear' }));
    expect(screen.getByRole('alert')).toHaveTextContent('Select at least one agent');
    expect(screen.getByRole('group', { name: 'Agents' })).toHaveAccessibleDescription('Select at least one agent.');
    expect(screen.getByRole('button', { name: 'Find conversations' })).toBeDisabled();
    await query();
    expect(queryMock).not.toHaveBeenCalled();
    toggleAgent('Owner B');
    await query();
    expect(queryMock.mock.calls[0][0].agentIds).toEqual(['Owner B']);
});

test('blocks queries if there are no available agents', async () => {
    render(<SessionBatchExportDialog open agents={[{ name: 'Unavailable', available: false }]} onOpenChange={jest.fn()} />);
    expect(screen.getByRole('alert')).toHaveTextContent('No available agents');
    await query();
    expect(queryMock).not.toHaveBeenCalled();
});

test('keeps default all live after agent discovery and invalidates prior results', async () => {
    const { rerender, onOpenChange } = show();
    await query();
    rerender(<SessionBatchExportDialog open agents={[...agents, { name: 'New owner', available: true }]} onOpenChange={onOpenChange} />);
    expect(screen.queryByText('Chat 0')).not.toBeInTheDocument();
    expect(screen.getByText('4 of 4 selected')).toBeInTheDocument();
    await query();
    expect(queryMock.mock.calls[1][0].agentIds).toEqual(['Owner A', 'Owner B', 'Unspecified availability', 'New owner']);
});

test('does not silently expand an explicit selection when an agent becomes unavailable', async () => {
    const { rerender, onOpenChange } = show();
    fireEvent.click(screen.getByRole('button', { name: 'Clear' }));
    toggleAgent('Owner A');
    rerender(<SessionBatchExportDialog open agents={[{ name: 'Owner A', available: false }, { name: 'Owner B', available: true }]} onOpenChange={onOpenChange} />);
    expect(screen.getByRole('alert')).toHaveTextContent('Select at least one agent');
    await query();
    expect(queryMock).not.toHaveBeenCalled();
});

test.each([{ label: 'Last day', days: 1 }, { label: 'Last week', days: 7 }, { label: 'Last month', days: 30 }])('serializes $label as server-clock rolling creation days only', async ({ label, days }) => {
    show();
    chooseCustom();
    choosePreset(label);
    expect(within(screen.getByRole('region', { name: 'Creation time' })).getByRole('button', { name: label })).toHaveAttribute('aria-pressed', 'true');
    await query();
    expect(queryMock.mock.calls[0][0]).toEqual({ agentIds: ['Owner A', 'Owner B', 'Unspecified availability'], createdDays: days, keyword: '' });
    expect(queryMock.mock.calls[0][0]).not.toHaveProperty('from');
    expect(queryMock.mock.calls[0][0]).not.toHaveProperty('to');
});

test('shows the local time zone and serializes custom endpoints to ISO without days', async () => {
    show();
    chooseCustom();
    expect(screen.getByRole('region', { name: 'Creation time' })).toBeInTheDocument();
    setRange('2026-10-01T09:30', '2026-10-28T18:15');
    fireEvent.change(titleInput(), { target: { value: 'ds' } });
    await query();
    expect(queryMock.mock.calls[0][0]).toEqual({ agentIds: ['Owner A', 'Owner B', 'Unspecified availability'], createdFrom: new Date('2026-10-01T09:30').toISOString(), createdTo: new Date('2026-10-28T18:15').toISOString(), keyword: 'ds' });
    expect(queryMock.mock.calls[0][0]).not.toHaveProperty('days');
});

test.each([
    ['', '2026-10-28T18:15'],
    ['2026-10-01T09:30', ''],
    ['2026-10-29T09:30', '2026-10-28T18:15'],
    ['2026-02-30T09:30', '2026-10-28T18:15'],
])('rejects invalid custom range %s to %s without submitting', async (from, to) => {
    show();
    chooseCustom();
    setRange(from, to);
    expect(screen.getByRole('alert')).toHaveTextContent('Enter a valid start and end time');
    expect(screen.getByLabelText('Creation start')).toHaveAttribute('aria-invalid', 'true');
    expect(screen.getByRole('button', { name: 'Find conversations' })).toBeDisabled();
    await query();
    expect(queryMock).not.toHaveBeenCalled();
});

test('accepts an inclusive custom range whose endpoints are equal', async () => {
    show();
    chooseCustom();
    setRange('2026-10-28T18:15', '2026-10-28T18:15');
    await query();
    expect(queryMock.mock.calls[0][0].createdFrom).toBe(queryMock.mock.calls[0][0].createdTo);
});

test('combines creation time presets and custom ranges with agent and title filters', async () => {
    show();
    toggleAgent('Unspecified availability');
    choosePreset('Last week');
    fireEvent.change(titleInput(), { target: { value: 'deepseek' } });
    await query();
    expect(queryMock).toHaveBeenLastCalledWith({
        agentIds: ['Owner A', 'Owner B'],
        createdDays: 7,
        keyword: 'deepseek',
    }, expect.any(AbortSignal));

    chooseCustom();
    setRange('2026-10-01T09:00', '2026-10-10T18:00');
    await query();
    expect(queryMock).toHaveBeenLastCalledWith({
        agentIds: ['Owner A', 'Owner B'],
        createdFrom: new Date('2026-10-01T09:00').toISOString(),
        createdTo: new Date('2026-10-10T18:00').toISOString(),
        keyword: 'deepseek',
    }, expect.any(AbortSignal));
});

test('exports all snapshot matches without keys even when viewing a later page', async () => {
    show();
    await query();
    expect(screen.getByText('Matching conversations: 12')).toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: 'Next page' }));
    expect(screen.getByText('Chat 11')).toBeInTheDocument();
    expect(screen.queryByText('Chat 0')).not.toBeInTheDocument();
    const document = artifact();
    statusMock.mockResolvedValue(ready({ phase: 'completed', document }));
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: 'Export all (12)' })); });
    expect(batchMock).toHaveBeenCalledWith({ jobId: 'job-1', persist: false }, expect.any(AbortSignal));
    expect(batchMock.mock.calls[0][0]).not.toHaveProperty('keys');
    expect(downloadMock).toHaveBeenCalledWith(document);
    expect(screen.getByRole('status')).toHaveTextContent('JSON download dispatched to your browser.');
    await tick();
    expect(downloadMock).toHaveBeenCalledTimes(1);
    expect(screen.getByRole('button', { name: 'Export all (12)' })).toBeDisabled();
});

test('retains an explicit selection across pages and exports only opaque selected keys', async () => {
    show();
    await query();
    fireEvent.click(screen.getByText('Chat 0'));
    fireEvent.click(screen.getByRole('button', { name: 'Next page' }));
    fireEvent.click(screen.getByLabelText('Select Chat 11 (Owner B)'));
    fireEvent.click(screen.getByRole('button', { name: 'Previous page' }));
    expect(screen.getByLabelText('Select Chat 0 (Owner A)')).toBeChecked();
    statusMock.mockResolvedValue(ready({ phase: 'completed', document: artifact() }));
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: 'Export selected (2)' })); });
    expect(batchMock.mock.calls[0][0]).toEqual({ jobId: 'job-1', keys: ['opaque-0', 'opaque-11'], persist: false });
});

test.each(['keyword', 'preset', 'owner', 'custom-from', 'custom-to'])('discards results and cross-page selection when %s changes', async (field) => {
    show();
    if (field.startsWith('custom')) chooseCustom();
    await query();
    fireEvent.click(screen.getByLabelText('Select Chat 0 (Owner A)'));
    fireEvent.click(screen.getByRole('button', { name: 'Next page' }));
    fireEvent.click(screen.getByLabelText('Select Chat 11 (Owner B)'));
    if (field === 'keyword') fireEvent.change(titleInput(), { target: { value: 'new' } });
    if (field === 'preset') choosePreset('Last week');
    if (field === 'owner') toggleAgent('Owner B');
    if (field === 'custom-from') fireEvent.change(screen.getByLabelText('Creation start'), { target: { value: '2026-10-01T12:00' } });
    if (field === 'custom-to') fireEvent.change(screen.getByLabelText('Creation end'), { target: { value: '2026-10-25T12:00' } });
    expect(screen.queryByText('Chat 11')).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Export selected (0)' })).toBeDisabled();
    expect(screen.getByRole('button', { name: 'Export all (0)' })).toBeDisabled();
    await query();
    expect(screen.getByText('Chat 0')).toBeInTheDocument();
});

test('polls actual unknown and known progress, then presents results and per-owner/session issues', async () => {
    statusMock.mockResolvedValueOnce(ready({ phase: 'scanning', processed: 3, total: null, items: [] }))
        .mockResolvedValueOnce(ready({ phase: 'scanning', processed: 9, total: 20, items: [] }))
        .mockResolvedValueOnce(ready({ issues: [{ agentId: 'Owner B', sessionId: 'broken', code: 'history_failed', message: 'History unavailable' }] }));
    show();
    await query();
    expect(screen.getByRole('status')).toHaveTextContent('3 processed; total not yet known');
    expect(screen.getByRole('button', { name: 'Find conversations' })).toBeDisabled();
    await tick();
    expect(screen.getByRole('status')).toHaveTextContent('9 / 20 processed');
    await tick();
    expect(screen.getByText('Matching conversations: 12')).toBeInTheDocument();
    expect(screen.getByRole('region', { name: 'Owners or sessions not included / issues' })).toHaveTextContent('Owner B / broken — history_failed: History unavailable');
    expect(screen.getByRole('button', { name: 'Export all (12)' })).toBeEnabled();
    await tick();
    expect(statusMock).toHaveBeenCalledTimes(3);
});

test('distinguishes genuine no-match results from unsupported histories', async () => {
    statusMock.mockResolvedValueOnce(ready({ items: [], processed: 0, total: 0 }));
    show();
    await query();
    expect(screen.getByText('No conversations match these filters.')).toBeInTheDocument();
    fireEvent.change(titleInput(), { target: { value: 'new' } });
    statusMock.mockResolvedValueOnce(ready({ items: [], issues: [{ agentId: 'Owner B', code: 'unsupported', message: 'History enumeration is unsupported' }] }));
    await query();
    expect(screen.queryByText('No conversations match these filters.')).not.toBeInTheDocument();
    expect(screen.getByText(/No exportable matches/)).toBeInTheDocument();
    expect(screen.getByText(/unsupported: History enumeration is unsupported/)).toBeInTheDocument();
});

test('shows a failed status truthfully without delivering an artifact', async () => {
    statusMock.mockResolvedValue(ready({ phase: 'failed', error: 'Desktop write failed', issues: [{ agentId: 'Owner A', code: 'save_failed', message: 'Read-only directory' }] }));
    show();
    await query();
    expect(screen.getByRole('alert')).toHaveTextContent('Desktop write failed');
    expect(screen.getByText(/save_failed: Read-only directory/)).toBeInTheDocument();
    expect(downloadMock).not.toHaveBeenCalled();
    await tick();
    expect(statusMock).toHaveBeenCalledTimes(1);
});

test.each([
    ready({ jobId: 'another-job' }),
    ready({ phase: 'completed' }),
])('rejects a mismatched job or missing completed document without delivery', async (status) => {
    statusMock.mockResolvedValue(status);
    show();
    await query();
    expect(screen.getByRole('alert')).toBeInTheDocument();
    expect(downloadMock).not.toHaveBeenCalled();
});

test('reports batch-request failure and lets the user retry the retained snapshot', async () => {
    batchMock.mockRejectedValueOnce(new Error('Export request denied'));
    show();
    await query();
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: 'Export all (12)' })); });
    expect(screen.getByRole('alert')).toHaveTextContent('Export request denied');
    expect(downloadMock).not.toHaveBeenCalled();
    expect(screen.getByRole('button', { name: 'Export all (12)' })).toBeEnabled();
    statusMock.mockResolvedValue(ready({ phase: 'completed', document: artifact() }));
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: 'Export all (12)' })); });
    expect(downloadMock).toHaveBeenCalledTimes(1);
});

test('locks filters and dismissal from export submission through delivery, even if discovery changes', async () => {
    const pending = deferred<{ jobId: string }>();
    batchMock.mockReturnValueOnce(pending.promise);
    const { rerender, onOpenChange } = show();
    chooseCustom();
    await query();
    fireEvent.click(screen.getByRole('button', { name: 'Export all (12)' }));
    const signal = batchMock.mock.calls[0][1] as AbortSignal;
    expect(agentCheckbox('Owner A')).toBeDisabled();
    expect(screen.getByRole('button', { name: 'Clear' })).toBeDisabled();
    expect(screen.getByLabelText('Creation start')).toBeDisabled();
    expect(titleInput()).toBeDisabled();
    expect(within(screen.getByRole('region', { name: 'Creation time' })).getByRole('button', { name: 'Last day' })).toBeDisabled();
    expect(screen.queryByRole('button', { name: 'Close' })).not.toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: 'Cancel' }));
    fireEvent.keyDown(screen.getByRole('dialog'), { key: 'Escape', keyCode: 27 });
    expect(onOpenChange).not.toHaveBeenCalled();

    rerender(<SessionBatchExportDialog open agents={[...agents, { name: 'New owner' }]} onOpenChange={onOpenChange} />);
    expect(signal.aborted).toBe(false);
    statusMock.mockResolvedValueOnce(ready({ phase: 'exporting' })).mockResolvedValue(ready({ phase: 'completed', document: artifact() }));
    await act(async () => { pending.resolve({ jobId: 'job-1' }); });
    expect(downloadMock).not.toHaveBeenCalled();
    expect(screen.getByRole('button', { name: 'Cancel' })).toBeDisabled();
    await tick();
    expect(downloadMock).toHaveBeenCalledTimes(1);
    expect(titleInput()).toBeEnabled();
    fireEvent.click(screen.getByRole('button', { name: 'Done' }));
    expect(onOpenChange).toHaveBeenCalledWith(false);
});

test('shows request and polling errors without false success', async () => {
    queryMock.mockRejectedValueOnce(new Error('Query denied'));
    show();
    await query();
    expect(screen.getByRole('alert')).toHaveTextContent('Query denied');
    statusMock.mockRejectedValueOnce(new Error('Status unavailable'));
    await query();
    expect(screen.getByRole('alert')).toHaveTextContent('Status unavailable');
    expect(downloadMock).not.toHaveBeenCalled();
});

test('blocks duplicate query and batch submits, and displays export progress until completion', async () => {
    const pendingQuery = deferred<{ jobId: string }>();
    queryMock.mockReturnValueOnce(pendingQuery.promise);
    show();
    await query();
    await query();
    expect(queryMock).toHaveBeenCalledTimes(1);
    await act(async () => { pendingQuery.resolve({ jobId: 'job-1' }); });
    const pendingBatch = deferred<{ jobId: string }>();
    batchMock.mockReturnValueOnce(pendingBatch.promise);
    fireEvent.click(screen.getByRole('button', { name: 'Export all (12)' }));
    expect(screen.getByRole('button', { name: 'Export all (12)' })).toBeDisabled();
    fireEvent.click(screen.getByRole('button', { name: 'Export all (12)' }));
    expect(batchMock).toHaveBeenCalledTimes(1);
    statusMock.mockResolvedValueOnce(ready({ phase: 'exporting', processed: 4, total: 12 })).mockResolvedValueOnce(ready({ phase: 'completed', document: artifact() }));
    await act(async () => { pendingBatch.resolve({ jobId: 'job-1' }); });
    expect(screen.getByRole('status')).toHaveTextContent('Exporting… 4 / 12 processed');
    await tick();
    expect(downloadMock).toHaveBeenCalledTimes(1);
});

test('ignores a stale query response after discovery changes and a newer query finishes', async () => {
    const stale = deferred<{ jobId: string }>();
    queryMock.mockReturnValueOnce(stale.promise).mockResolvedValueOnce({ jobId: 'new-job' });
    statusMock.mockResolvedValue(ready({ jobId: 'new-job', items: [] }));
    const { rerender, onOpenChange } = show();
    await query();
    const oldSignal = queryMock.mock.calls[0][1] as AbortSignal;
    expect(titleInput()).toBeDisabled();
    rerender(<SessionBatchExportDialog open agents={[...agents, { name: 'New owner' }]} onOpenChange={onOpenChange} />);
    expect(oldSignal.aborted).toBe(true);
    await query();
    await act(async () => { stale.resolve({ jobId: 'old-job' }); });
    expect(statusMock).toHaveBeenCalledTimes(1);
    expect(statusMock.mock.calls[0][0]).toBe('new-job');
    expect(screen.queryByText('Chat 0')).not.toBeInTheDocument();
});

test('aborts in-flight status and ignores its late completion when closed', async () => {
    const pending = deferred<SessionExportStatus>();
    statusMock.mockReturnValueOnce(pending.promise);
    const { rerender, onOpenChange } = show();
    await query();
    const signal = statusMock.mock.calls[0][1] as AbortSignal;
    fireEvent.click(screen.getByRole('button', { name: 'Close' }));
    expect(onOpenChange).toHaveBeenCalledWith(false);
    rerender(<SessionBatchExportDialog open={false} agents={agents} onOpenChange={onOpenChange} />);
    expect(signal.aborted).toBe(true);
    await act(async () => { pending.resolve(ready({ phase: 'completed', document: artifact() })); });
    await tick();
    expect(downloadMock).not.toHaveBeenCalled();
    expect(statusMock).toHaveBeenCalledTimes(1);
});

test('clears a scheduled poll on unmount and reopens without stale selection', async () => {
    statusMock.mockResolvedValue(ready({ phase: 'scanning', items: [] }));
    const { rerender, onOpenChange } = show();
    await query();
    rerender(<SessionBatchExportDialog open={false} agents={agents} onOpenChange={onOpenChange} />);
    await tick();
    expect(statusMock).toHaveBeenCalledTimes(1);
    rerender(<SessionBatchExportDialog open agents={agents} onOpenChange={onOpenChange} />);
    expect(screen.getByRole('button', { name: 'Export selected (0)' })).toBeDisabled();
    expect(screen.getByText('3 of 3 selected')).toBeInTheDocument();
});

test('ignores a stale query status response when discovery changes', async () => {
    const pending = deferred<SessionExportStatus>();
    statusMock.mockReturnValueOnce(pending.promise);
    const { rerender, onOpenChange } = show();
    await query();
    expect(agentCheckbox('Owner A')).toBeDisabled();
    rerender(<SessionBatchExportDialog open agents={[...agents, { name: 'New owner' }]} onOpenChange={onOpenChange} />);
    await act(async () => { pending.resolve(ready()); });
    expect(screen.queryByText('Chat 0')).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Export all (0)' })).toBeDisabled();
});

test('requests desktop persistence and exposes the full saved path in a focusable tooltip', async () => {
    persistMock.mockReturnValue(true);
    show();
    await query();
    const savedPath = '/Users/example/Downloads/session-exports/insight-sessions-6a3252f0-63f2-4119-a481-b20210424917.json';
    const document = artifact(savedPath);
    statusMock.mockResolvedValue(ready({ phase: 'completed', document }));
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: 'Export all (12)' })); });
    expect(batchMock.mock.calls[0][0].persist).toBe(true);
    expect(downloadMock).toHaveBeenCalledWith(document);
    expect(screen.getByRole('status')).toHaveTextContent(`Export saved: ${savedPath}`);
    expect(screen.getByRole('status')).not.toHaveTextContent('download dispatched');
    const path = screen.getByText(savedPath);
    expect(path).toHaveAttribute('tabindex', '0');
    fireEvent.focus(path);
    await tick();
    expect(screen.getByRole('tooltip')).toHaveTextContent(savedPath);
});

test('reports an actual failed desktop export phase without download fallback', async () => {
    persistMock.mockReturnValue(true);
    show();
    await query();
    statusMock.mockResolvedValue(ready({ phase: 'failed', error: 'Could not save batch to Downloads', issues: [{ agentId: 'Owner A', code: 'save_failed', message: 'Permission denied' }] }));
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: 'Export all (12)' })); });
    expect(batchMock.mock.calls[0][0].persist).toBe(true);
    expect(screen.getByRole('alert')).toHaveTextContent('Could not save batch to Downloads');
    expect(screen.getByText(/save_failed: Permission denied/)).toBeInTheDocument();
    expect(downloadMock).not.toHaveBeenCalled();
    expect(screen.getByRole('status')).not.toHaveTextContent('Export saved');
});

test('never falls back to a browser download when desktop completion lacks savedPath', async () => {
    persistMock.mockReturnValue(true);
    show();
    await query();
    statusMock.mockResolvedValue(ready({ phase: 'completed', document: artifact() }));
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: 'Export all (12)' })); });
    expect(screen.getByRole('alert')).toHaveTextContent('Desktop export was not saved');
    expect(screen.getByRole('status')).not.toHaveTextContent('Export saved');
    expect(downloadMock).not.toHaveBeenCalled();
});
