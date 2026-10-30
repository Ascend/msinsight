/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React from 'react';
import { observable, runInAction } from 'mobx';
// eslint-disable-next-line import/named -- waitFor is re-exported from @testing-library/dom.
import { act, fireEvent, render, waitFor } from '@testing-library/react';
import { ResizeTable } from '@insight/lib/resize';
import { queryFreeAttributionDetails, queryFreeAttributionSummary } from '../../../api/request';
import type { CardRankInfo, FreeAttributionDetails, FreeAttributionSummary, OpDetail } from '../../../api/interface';
import jumpToUnitOperator from '../../../utils/jumpToUnitOperator';
import { FreeAttribution } from '../FreeAttribution';
import type { SelectContentViewProps } from '../SystemView';
import { freeAttributionTimeRange, freeTargetStart, freeTimeUs } from '../FreeAttributionUtils';

jest.mock('react-i18next', () => {
    const translations = jest.requireActual('../../../../../lib/src/i18n/timeline/en.json').timeline.freeAttribution;
    return {
        useTranslation: () => ({
            t: (key: string, options?: { value?: string; count?: number; defaultValue?: string }) => {
                if (options?.count !== undefined) return `${options.value} (${options.count})`;
                const translation = key.split('.').reduce((value, part) => value?.[part], translations);
                return translation === undefined ? options?.defaultValue ?? key : key;
            },
        }),
    };
});
jest.mock('@insight/lib/components', () => ({
    Button: ({ children, type, ...props }: any) => <button {...props}>{children}</button>,
    Tooltip: ({ children, title }: any) => <span title={title}>{children}</span>,
}));
jest.mock('@insight/lib/resize', () => ({ ResizeTable: jest.fn(), fetchColumnFilterProps: () => ({}) }));
jest.mock('../../../api/request', () => ({
    queryFreeAttributionSummary: jest.fn(), queryFreeAttributionDetails: jest.fn(),
}));
jest.mock('../../../utils/jumpToUnitOperator', () => ({ __esModule: true, default: jest.fn() }));
jest.mock('../SystemView', () => ({ DETAIL_HEADER_HEIGHT_ETC_PX: 120 }));
jest.mock('../Common', () => ({ getPageData: (page: unknown) => page }));

const summary: FreeAttributionSummary = {
    groups: [{ groupId: 'api:aten::add', groupType: 'matched', apiName: 'aten::add', totalFreeDuration: 1500, freeCount: 1 }],
    totalGroups: 1,
    totalFreeDuration: 1500,
    totalFreeCount: 1,
};
const hostApi: OpDetail = {
    id: 'api-1',
    name: 'aten::add',
    cardId: 'Host',
    dbPath: 'source.db',
    pid: '1',
    tid: '2',
    depth: 1,
    timestamp: 1500,
    duration: 500,
    metaType: 'Python',
};
const free: OpDetail = {
    id: 'free-1',
    name: 'Free',
    cardId: 'rank',
    dbPath: 'overlap.db',
    pid: 'Overlap Analysis',
    tid: 'Free',
    depth: 0,
    timestamp: 2000,
    duration: 1500,
    metaType: 'TEXT',
};
const cardInfo = (rankId: string, dbPath: string): CardRankInfo => ({
    rankInfo: { clusterId: '', host: '', rankName: rankId, rankId, deviceId: '0' }, dbPath,
});

const registerCard = (session: SelectContentViewProps['session'], rankId: string, dbPath: string): void => {
    session.rankCardInfoMap.clear();
    session.rankCardInfoMap.set(rankId, cardInfo(rankId, dbPath));
};

const makeProps = (): SelectContentViewProps => ({
    key: 'test',
    card: { cardId: 'rank', dbPath: 'source.db' },
    bottomHeight: 600,
    session: {
        asyncDataLoadingList: {},
        rankCardInfoMap: new Map([['rank', cardInfo('rank', 'source.db')]]),
        units: [{ metadata: { cardId: 'rank', dbPath: 'source.db' }, phase: 'download' }],
        unitsConfig: { offsetConfig: { timestampOffset: { rank__device: 20, Host__host: 100 } } },
        timeAnalysisRange: [100, 200],
    },
} as unknown as SelectContentViewProps);

const setOverlapState = (session: SelectContentViewProps['session'], dbId: string, version: number): void => {
    act(() => runInAction(() => {
        session.asyncDataLoadingList[dbId] = { OVERLAP_ANALYSIS: true, OVERLAP_ANALYSIS_VERSION: version };
    }));
};

const latestSummaryTable = (): any => (ResizeTable as jest.Mock).mock.calls.map(call => call[0])
    .filter(table => table.rowKey === 'groupId').pop();

beforeEach(() => {
    (queryFreeAttributionSummary as jest.Mock).mockResolvedValue(summary);
    (queryFreeAttributionDetails as jest.Mock).mockResolvedValue({
        records: [{
            detailId: 'free-1',
            freeDuration: 1500,
            nextBehaviorType: 'Computing',
            attributionStatus: 'matched',
            reason: '',
            hostApiTarget: hostApi,
            freeTarget: free,
            candidateApiTargets: [],
        }],
        totalRecords: 1,
    });
    (ResizeTable as jest.Mock).mockImplementation(({ dataSource, columns, expandable, rowKey }: any) => <div>
        {dataSource.map((row: any) => <div key={row[rowKey]}>
            {expandable && <button onClick={() => expandable.onExpandedRowsChange([row.groupId])}>Expand {row.apiName}</button>}
            {columns.map((column: any, index: number) => <span key={index}>
                {column.render ? column.render(row[column.dataIndex], row) : row[column.dataIndex]}
            </span>)}
            {expandable?.expandedRowKeys.includes(row.groupId) && expandable.expandedRowRender(row)}
        </div>)}
    </div>);
});

it('converts the selected range using the device offset and preserves whole-event requests', async () => {
    const view = render(<FreeAttribution {...makeProps()} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledWith(expect.objectContaining({
        rankId: 'rank', rangeMode: 'range', startTime: 120, endTime: 220, orderBy: 'totalFreeDuration', order: 'desc',
    })));
    fireEvent.click(await view.findByText('Expand aten::add'));
    await waitFor(() => expect(queryFreeAttributionDetails).toHaveBeenCalledWith(expect.objectContaining({
        groupId: 'api:aten::add', rangeMode: 'range', startTime: 120, endTime: 220, orderBy: 'freeStartTime', order: 'asc',
    })));
    // Host and Free use their own lane offsets, and wire targets remain unchanged.
    expect(await view.findByText('1.4')).toBeInTheDocument();
    expect(view.getByText('1.98')).toBeInTheDocument();
    fireEvent.click(view.getByText('Jump API'));
    expect(jumpToUnitOperator).toHaveBeenLastCalledWith(hostApi);
    fireEvent.click(view.getByText('Jump Free'));
    expect(jumpToUnitOperator).toHaveBeenLastCalledWith(free);
    expect(makeProps().session.timeAnalysisRange).toEqual([100, 200]);
    const detailTable = (ResizeTable as jest.Mock).mock.calls.map(call => call[0]).find(props => props.rowKey === 'detailId');
    expect(detailTable.copyColumns.every((column: any) => column.key !== 'actions')).toBe(true);
    expect(detailTable.copyColumns.map((column: any) => column.dataIndex)).toEqual([
        'apiName', 'apiStartTime', 'apiDuration', 'freeStartTime', 'freeDurationLabel', 'nextBehaviorType', 'statusLabel',
    ]);
    expect(detailTable.columns.find((column: any) => column.dataIndex === 'statusLabel').width).toBe(220);
    expect(await view.findByText('status.matched')).toBeInTheDocument();
    const summaryTable = (ResizeTable as jest.Mock).mock.calls.map(call => call[0]).find(props => props.rowKey === 'groupId' && props.dataSource.length);
    expect(summaryTable.columns.find((column: any) => column.dataIndex === 'totalFreeDuration').sortOrder).toBe('descend');
    expect(detailTable.columns.find((column: any) => column.dataIndex === 'freeStartTime').sortOrder).toBe('ascend');
    expect(summaryTable.copyColumns.every((column: any) => typeof column.title === 'string')).toBe(true);
});

it.each([
    ['matched', 'multipleApis', 'status.matched'],
    ['ambiguous', 'multipleApis', 'reason.multipleApis'],
    ['incomplete', 'incompleteFlow', 'reason.incompleteFlow'],
    ['unresolved', 'noFlow', 'reason.noFlow'],
    ['unresolved', '', 'status.unresolved'],
    ['incomplete', 'unknownReason', 'status.incomplete'],
])('merges detail status %s and reason %s into one displayed and copied value', async (attributionStatus, reason, label) => {
    (queryFreeAttributionDetails as jest.Mock).mockResolvedValue({
        records: [{
            detailId: 'free-1',
            freeDuration: 1500,
            nextBehaviorType: 'Computing',
            attributionStatus,
            reason,
            hostApiTarget: null,
            freeTarget: free,
            candidateApiTargets: [],
        }],
        totalRecords: 1,
    });
    const view = render(<FreeAttribution {...makeProps()} />);
    fireEvent.click(await view.findByText('Expand aten::add'));
    expect((await view.findByText(label)).parentElement).toHaveAttribute('title', label);
    const detailTable = (ResizeTable as jest.Mock).mock.calls.map(call => call[0])
        .filter(props => props.rowKey === 'detailId').pop();
    expect(detailTable.dataSource[0].statusLabel).toBe(label);
    expect(detailTable.columns.map((column: any) => column.dataIndex)).not.toContain('reasonLabel');
    expect(detailTable.copyColumns.map((column: any) => column.dataIndex)).not.toContain('reasonLabel');
});

it('requests server sorting and exposes each ambiguous API target separately', async () => {
    const anotherApi = { ...hostApi, id: 'api-2', name: 'aten::mul', timestamp: 2500 };
    (queryFreeAttributionDetails as jest.Mock).mockResolvedValue({
        records: [{
            detailId: 'free-1',
            freeDuration: 1500,
            nextBehaviorType: 'Computing',
            attributionStatus: 'ambiguous',
            reason: 'multipleApis',
            hostApiTarget: null,
            freeTarget: free,
            candidateApiTargets: [hostApi, anotherApi],
        }],
        totalRecords: 1,
    });
    const view = render(<FreeAttribution {...makeProps()} />);
    fireEvent.click(await view.findByText('Expand aten::add'));
    fireEvent.click(await view.findByText('Jump API 1'));
    expect(jumpToUnitOperator).toHaveBeenLastCalledWith(hostApi);
    expect(view.getByText('aten::add (2)').parentElement).toHaveAttribute('title', 'aten::add / aten::mul');
    expect(view.getByText('1.4 (2)').parentElement).toHaveAttribute('title', '1.4 / 2.4');
    expect(view.getByText('0.5 (2)').parentElement).toHaveAttribute('title', '0.5 / 0.5');
    const detailTable = (ResizeTable as jest.Mock).mock.calls.map(call => call[0])
        .filter(props => props.rowKey === 'detailId').pop();
    expect(detailTable.dataSource[0]).toEqual(expect.objectContaining({
        apiName: 'aten::add / aten::mul', apiStartTime: '1.4 / 2.4', apiDuration: '0.5 / 0.5',
    }));
    expect(detailTable.copyColumns.slice(0, 3).every((column: any) => column.render === undefined)).toBe(true);
    fireEvent.click(view.getByText('Jump API 2'));
    expect(jumpToUnitOperator).toHaveBeenLastCalledWith(anotherApi);
    expect(view.getByText('Jump API 2').title).toContain('PID/TID: 1/2');
    expect(view.getByText('Jump API 2').title).toContain('Source: source.db');
    expect(view.getByText('Jump API 2').title).toContain('2.4');
    const table = (ResizeTable as jest.Mock).mock.calls.map(call => call[0]).filter(props => props.rowKey === 'groupId').pop();
    act(() => table.onChange({}, {}, { field: 'freeCount', order: 'ascend' }, { action: 'sort' }));
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({
        orderBy: 'freeCount', order: 'asc', current: 1,
    })));
});

it('discards an old response after the card or analysis range changes', async () => {
    let finishOld!: (value: FreeAttributionSummary) => void;
    (queryFreeAttributionSummary as jest.Mock).mockImplementationOnce(() => new Promise(resolve => { finishOld = resolve; }));
    const props = makeProps();
    const view = render(<FreeAttribution {...props} />);
    const updated = {
        ...props,
        card: { cardId: 'rank-new', dbPath: 'new.db' },
        session: {
            ...props.session,
            rankCardInfoMap: new Map([['rank-new', cardInfo('rank-new', 'new.db')]]),
            units: [{ metadata: { cardId: 'rank-new', dbPath: 'new.db' }, phase: 'download' }],
        } as unknown as SelectContentViewProps['session'],
    };
    view.rerender(<FreeAttribution {...updated} />);
    await view.findByText('Expand aten::add');
    await act(async () => finishOld({ ...summary, groups: [{ ...summary.groups[0], apiName: 'stale-api' }] }));
    expect(view.queryByText('stale-api')).not.toBeInTheDocument();
});

it('uses an explicit all-data mode and formats nanoseconds consistently as microseconds', () => {
    expect(freeAttributionTimeRange(undefined, 20)).toEqual({ rangeMode: 'all', startTime: 0, endTime: 0 });
    expect(freeAttributionTimeRange(undefined, 20.5)).toEqual({ rangeMode: 'all', startTime: 0, endTime: 0 });
    expect(freeAttributionTimeRange([-20, 30], 10)).toEqual({ rangeMode: 'range', startTime: 0, endTime: 40 });
    expect(freeTimeUs(0)).toBe('0');
    expect(freeTimeUs(10000000)).toBe('10000');
    expect(freeTimeUs(1501)).toBe('1.501');
});

it('preserves the bounded zero point in summary and detail requests', async () => {
    const props = makeProps();
    props.session.timeAnalysisRange = [0, 100];
    props.session.unitsConfig.offsetConfig.timestampOffset = { rank__device: -100 };
    const view = render(<FreeAttribution {...props} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledWith(expect.objectContaining({
        rangeMode: 'range', startTime: 0, endTime: 0,
    })));
    fireEvent.click(await view.findByText('Expand aten::add'));
    await waitFor(() => expect(queryFreeAttributionDetails).toHaveBeenCalledWith(expect.objectContaining({
        rangeMode: 'range', startTime: 0, endTime: 0, groupId: 'api:aten::add',
    })));
});

it('invalidates all, bounded and empty contexts even when their wire boundaries are all zero', async () => {
    let finishBounded!: (value: FreeAttributionSummary) => void;
    const empty: FreeAttributionSummary = { groups: [], totalGroups: 0, totalFreeDuration: 0, totalFreeCount: 0 };
    (queryFreeAttributionSummary as jest.Mock)
        .mockResolvedValueOnce(summary)
        .mockImplementationOnce(() => new Promise(resolve => { finishBounded = resolve; }))
        .mockResolvedValueOnce(empty);
    const props = makeProps();
    props.session.timeAnalysisRange = undefined;
    props.session.unitsConfig.offsetConfig.timestampOffset = { rank__device: -100 };
    const view = render(<FreeAttribution {...props} />);
    await view.findByText('Expand aten::add');
    expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({
        rangeMode: 'all', startTime: 0, endTime: 0,
    }));
    view.rerender(<FreeAttribution {...props} session={{ ...props.session, timeAnalysisRange: [0, 100] } as typeof props.session} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({
        rangeMode: 'range', startTime: 0, endTime: 0,
    })));
    view.rerender(<FreeAttribution {...props} session={{ ...props.session, timeAnalysisRange: [0, 99] } as typeof props.session} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({
        rangeMode: 'empty', startTime: 0, endTime: 0,
    })));
    await act(async () => finishBounded({ ...summary, groups: [{ ...summary.groups[0], apiName: 'stale-zero-api' }] }));
    expect(view.queryByText('Expand aten::add')).not.toBeInTheDocument();
    expect(view.queryByText('stale-zero-api')).not.toBeInTheDocument();
    const latestTable = (ResizeTable as jest.Mock).mock.calls.map(call => call[0])
        .filter(table => table.rowKey === 'groupId').pop();
    expect(latestTable.dataSource).toEqual([]);
    expect(latestTable.pagination.total).toBe(0);
});

it('uses a source-path host identity to resolve its own host-side offset', () => {
    const props = makeProps();
    const source = 'host/source.db';
    props.session.units = [{ metadata: { cardId: source } }] as unknown as typeof props.session.units;
    props.session.unitsConfig.offsetConfig.timestampOffset = { [`${source}__host`]: 500, [`${source}__device`]: 30 };
    expect(freeTargetStart(props.session, { ...hostApi, cardId: source, metaType: 'PYTORCH_API' })).toBe('1');
});

it.each([undefined, false, true])('refreshes an early empty result when Overlap completes from %s', async completed => {
    const props = makeProps();
    if (completed !== undefined) {
        props.session.asyncDataLoadingList['source.db'] = { OVERLAP_ANALYSIS: completed, OVERLAP_ANALYSIS_VERSION: 1 };
    }
    props.session = observable(props.session);
    (queryFreeAttributionSummary as jest.Mock).mockResolvedValueOnce({
        groups: [], totalGroups: 0, totalFreeDuration: 0, totalFreeCount: 0,
    });
    const view = render(<FreeAttribution {...props} />);
    await waitFor(() => expect(latestSummaryTable().loading).toBe(false));
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1);
    expect(view.queryByText('Expand aten::add')).not.toBeInTheDocument();

    // No rerender or user interaction: the observed task state must refresh the view itself.
    setOverlapState(props.session, 'source.db', 2);
    await view.findByText('Expand aten::add');
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(2);
});

it('accepts rank-keyed baseline completion without waiting for a source-path notification', async () => {
    const props = makeProps();
    props.session = observable(props.session);
    const view = render(<FreeAttribution {...props} />);
    await view.findByText('Expand aten::add');
    setOverlapState(props.session, 'rank', 1);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(2));
});

it('tracks every raw source of a merged card and ignores unrelated or derived-source completion', async () => {
    const props = makeProps();
    props.session.units = [{
        metadata: { cardId: 'rank', dbPath: 'source.db' },
        phase: 'download',
        children: [
            { metadata: { cardId: 'rank', dbPath: 'secondary.db', metaType: 'Ascend Hardware' } },
            { metadata: { cardId: 'rank', dbPath: 'file:derived-overlap', metaType: 'OVERLAP_ANALYSIS' } },
        ],
    }] as unknown as typeof props.session.units;
    props.session = observable(props.session);
    const view = render(<FreeAttribution {...props} />);
    await view.findByText('Expand aten::add');
    fireEvent.click(view.getByText('Expand aten::add'));
    await view.findByText('Jump API');

    setOverlapState(props.session, 'unrelated.db', 1);
    setOverlapState(props.session, 'file:derived-overlap', 1);
    act(() => runInAction(() => {
        props.session.asyncDataLoadingList['secondary.db'] = { OVERLAP_ANALYSIS: false, WAIT_TIME: true };
    }));
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1);
    expect(view.getByText('Jump API')).toBeInTheDocument();

    setOverlapState(props.session, 'secondary.db', 1);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(2));
    expect(view.queryByText('Jump API')).not.toBeInTheDocument();
});

it('discards a summary response started before the Overlap completion', async () => {
    let finishOld!: (value: FreeAttributionSummary) => void;
    (queryFreeAttributionSummary as jest.Mock).mockImplementationOnce(() => new Promise(resolve => { finishOld = resolve; }));
    const props = makeProps();
    props.session = observable(props.session);
    const view = render(<FreeAttribution {...props} />);
    setOverlapState(props.session, 'source.db', 1);
    await view.findByText('Expand aten::add');
    await act(async () => finishOld({ ...summary, groups: [{ ...summary.groups[0], apiName: 'stale-summary' }] }));
    expect(view.queryByText('stale-summary')).not.toBeInTheDocument();
    expect(view.getByText('Expand aten::add')).toBeInTheDocument();
});

it('resets detail state and rejects pending detail responses when Overlap completes', async () => {
    let finishOld!: (value: FreeAttributionDetails) => void;
    (queryFreeAttributionDetails as jest.Mock).mockImplementationOnce(() => new Promise(resolve => { finishOld = resolve; }));
    const props = makeProps();
    props.session = observable(props.session);
    const view = render(<FreeAttribution {...props} />);
    fireEvent.click(await view.findByText('Expand aten::add'));
    await waitFor(() => expect(queryFreeAttributionDetails).toHaveBeenCalledTimes(1));

    setOverlapState(props.session, 'source.db', 1);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(2));
    await act(async () => finishOld({
        records: [{
            detailId: 'stale-detail',
            freeDuration: 1500,
            nextBehaviorType: 'Computing',
            attributionStatus: 'matched',
            reason: '',
            hostApiTarget: { ...hostApi, name: 'stale-api' },
            freeTarget: free,
            candidateApiTargets: [],
        }],
        totalRecords: 1,
    }));
    expect(view.queryByText('stale-api')).not.toBeInTheDocument();
    expect(view.queryByText('Jump API')).not.toBeInTheDocument();
    fireEvent.click(await view.findByText('Expand aten::add'));
    await view.findByText('Jump API');
    expect(queryFreeAttributionDetails).toHaveBeenCalledTimes(2);
});

it('ignores completion for the previous card after switching cards', async () => {
    const props = makeProps();
    props.session = observable(props.session);
    const view = render(<FreeAttribution {...props} />);
    await view.findByText('Expand aten::add');
    act(() => runInAction(() => {
        props.session.units = [{ metadata: { cardId: 'rank-new', dbPath: 'new.db' }, phase: 'download' }] as unknown as typeof props.session.units;
        registerCard(props.session, 'rank-new', 'new.db');
    }));
    view.rerender(<FreeAttribution {...props} card={{ cardId: 'rank-new', dbPath: 'new.db' }} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(2));
    fireEvent.click(await view.findByText('Expand aten::add'));
    await view.findByText('Jump API');

    setOverlapState(props.session, 'source.db', 1);
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(2);
    expect(view.getByText('Jump API')).toBeInTheDocument();
    setOverlapState(props.session, 'new.db', 1);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(3));
    expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({ rankId: 'rank-new', dbPath: 'new.db' }));
});

it.each([undefined, 'configuring', 'analyzing', 'error'])('waits for a parsed card instead of querying phase %s', phase => {
    const props = makeProps();
    props.session.units[0].phase = phase as typeof props.session.units[0]['phase'];
    render(<FreeAttribution {...props} />);
    expect(queryFreeAttributionSummary).not.toHaveBeenCalled();
});

it('waits through a cleared session and a reused rank until the selected source is ready', async () => {
    const props = makeProps();
    props.session = observable(props.session);
    const view = render(<FreeAttribution {...props} />);
    await view.findByText('Expand aten::add');

    act(() => runInAction(() => {
        props.session.units = [];
        props.session.doReset = true;
        props.session.rankCardInfoMap.clear();
    }));
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1);
    expect(view.queryByText('Expand aten::add')).not.toBeInTheDocument();

    act(() => runInAction(() => {
        props.session.units = [{ metadata: { cardId: 'rank', dbPath: 'next.db' }, phase: 'analyzing' }] as unknown as typeof props.session.units;
    }));
    view.rerender(<FreeAttribution {...props} card={{ cardId: 'rank', dbPath: 'next.db' }} />);
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1);

    act(() => runInAction(() => {
        props.session.units[0].phase = 'download';
        registerCard(props.session, 'rank', 'next.db');
    }));
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(2));
    expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({ rankId: 'rank', dbPath: 'next.db' }));
});

it('does not query a stale database selection when the next project reuses the same rank ID', async () => {
    const props = makeProps();
    props.session.units[0].metadata.dbPath = 'next.db';
    registerCard(props.session, 'rank', 'next.db');
    const view = render(<FreeAttribution {...props} />);
    expect(queryFreeAttributionSummary).not.toHaveBeenCalled();

    view.rerender(<FreeAttribution {...props} card={{ cardId: 'rank', dbPath: 'next.db' }} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1));
    expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({ dbPath: 'next.db' }));
});

it('accepts a selected raw source belonging to a parsed merged card', async () => {
    const props = makeProps();
    props.card.dbPath = 'secondary.db';
    registerCard(props.session, 'rank', 'secondary.db');
    props.session.units[0].children = [{
        metadata: { cardId: 'rank', dbPath: 'secondary.db', metaType: 'Ascend Hardware' },
    }] as unknown as typeof props.session.units[0]['children'];
    render(<FreeAttribution {...props} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledWith(expect.objectContaining({ dbPath: 'secondary.db' })));
});

it('ignores MFU and baseline display resets but refreshes after the card is cleared and registered again', async () => {
    const props = makeProps();
    props.session = observable(props.session);
    const view = render(<FreeAttribution {...props} />);
    await view.findByText('Expand aten::add');
    act(() => runInAction(() => { props.session.kernelMfuProjectGeneration = 1; }));
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1);

    act(() => runInAction(() => { props.session.doReset = true; }));
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1);
    expect(view.getByText('Expand aten::add')).toBeInTheDocument();

    const cached = props.session.units[0];
    act(() => runInAction(() => {
        props.session.units = [];
        props.session.rankCardInfoMap.clear();
    }));
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1);
    expect(view.queryByText('Expand aten::add')).not.toBeInTheDocument();
    act(() => runInAction(() => {
        props.session.units = [cached];
        registerCard(props.session, 'rank', 'source.db');
    }));
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(2));
});

it('discards a failed request from the previous source and keeps errors retryable for the current source', async () => {
    let failOld!: (reason: Error) => void;
    (queryFreeAttributionSummary as jest.Mock).mockImplementationOnce(() => new Promise((_resolve, reject) => { failOld = reject; }));
    const props = makeProps();
    props.session = observable(props.session);
    const view = render(<FreeAttribution {...props} />);

    act(() => runInAction(() => { props.session.units = []; }));
    await act(async () => failOld(new Error('Old data source was released.')));
    expect(view.queryByRole('alert')).not.toBeInTheDocument();

    (queryFreeAttributionSummary as jest.Mock).mockRejectedValueOnce(new Error('Current query failed.'));
    act(() => runInAction(() => {
        props.session.units = [{ metadata: { cardId: 'rank', dbPath: 'source.db' }, phase: 'download' }] as unknown as typeof props.session.units;
    }));
    await view.findByRole('alert');
    fireEvent.click(view.getByRole('button', { name: 'Retry' }));
    await view.findByText('Expand aten::add');
    expect(view.queryByRole('alert')).not.toBeInTheDocument();
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(3);
});

it('waits for current registrations when a project switch retains old downloaded units', async () => {
    const props = makeProps();
    props.session.projectName = 'previous-project';
    props.session = observable(props.session);
    const view = render(<FreeAttribution {...props} />);
    await view.findByText('Expand aten::add');

    act(() => runInAction(() => {
        props.session.projectName = 'next-project';
        props.session.rankCardInfoMap.clear();
    }));
    expect(props.session.units[0].phase).toBe('download');
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1);
    expect(view.queryByText('Expand aten::add')).not.toBeInTheDocument();

    act(() => runInAction(() => {
        props.session.units.push({
            metadata: { cardId: 'rank-new', dbPath: 'next.db' }, phase: 'download',
        } as unknown as typeof props.session.units[0]);
        registerCard(props.session, 'rank-new', 'next.db');
    }));
    expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1);
    view.rerender(<FreeAttribution {...props} card={{ cardId: 'rank-new', dbPath: 'next.db' }} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(2));
    expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({ rankId: 'rank-new', dbPath: 'next.db' }));
});

it('accepts a registered comparison card from a different project', async () => {
    const props = makeProps();
    props.session.projectName = 'current-project';
    props.session.units[0].metadata.dataSource = {
        remote: 'local', port: 0, projectName: 'baseline-project', dataPath: [], projectPath: [], children: [],
    };
    render(<FreeAttribution {...props} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledTimes(1));
});
