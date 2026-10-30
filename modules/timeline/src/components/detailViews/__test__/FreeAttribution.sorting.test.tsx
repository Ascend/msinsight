/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React from 'react';
// eslint-disable-next-line import/named -- waitFor is re-exported from @testing-library/dom.
import { act, fireEvent, render, waitFor } from '@testing-library/react';
import { queryFreeAttributionDetails, queryFreeAttributionSummary } from '../../../api/request';
import type { FreeAttributionSummary, OpDetail } from '../../../api/interface';
import { FreeAttribution } from '../FreeAttribution';
import type { SelectContentViewProps } from '../SystemView';

jest.mock('react-i18next', () => ({
    useTranslation: () => ({
        t: (key: string, options?: { value?: string; count?: number }) => options?.count === undefined
            ? key
            : `${options.value} (${options.count})`,
    }),
}));
jest.mock('@insight/lib/components', () => ({
    Button: ({ children, type, icon, size, ...props }: any) => <button {...props}>{children}</button>,
    Input: jest.requireActual('antd').Input,
    Tooltip: ({ children }: any) => <span>{children}</span>,
}));
jest.mock('../../../api/request', () => ({
    queryFreeAttributionSummary: jest.fn(), queryFreeAttributionDetails: jest.fn(),
}));
jest.mock('../../../utils/jumpToUnitOperator', () => ({ __esModule: true, default: jest.fn() }));
jest.mock('../SystemView', () => ({ DETAIL_HEADER_HEIGHT_ETC_PX: 120 }));
jest.mock('../Common', () => ({
    getPageData: (page: { current: number; pageSize: number; total: number }, setPage: (value: unknown) => void) => ({
        ...page,
        showSizeChanger: false,
        showQuickJumper: false,
        onChange: (current: number, pageSize: number) => setPage({ ...page, current, pageSize }),
    }),
}));

const summary: FreeAttributionSummary = {
    groups: [{ groupId: 'matched:aten::add', groupType: 'matched', apiName: 'aten::add', totalFreeDuration: 1500, freeCount: 1 }],
    totalGroups: 25,
    totalFreeDuration: 1500,
    totalFreeCount: 1,
};
const hostApi: OpDetail = {
    id: 'api-1',
    name: 'aten::add',
    cardId: 'rank',
    dbPath: 'source.db',
    pid: '1',
    tid: 'pytorch',
    depth: 0,
    timestamp: 1500,
    duration: 500,
    metaType: 'PYTORCH_API',
};
const free: OpDetail = {
    id: 'free-1',
    name: 'Free',
    cardId: 'rank',
    dbPath: 'source.db',
    pid: 'OVERLAP_ANALYSIS',
    tid: '3',
    depth: 0,
    timestamp: 2000,
    duration: 1500,
    metaType: 'OVERLAP_ANALYSIS',
};
const makeProps = (): SelectContentViewProps => ({
    key: 'test',
    card: { cardId: 'rank', dbPath: 'source.db' },
    bottomHeight: 600,
    session: {
        asyncDataLoadingList: {},
        rankCardInfoMap: new Map([['rank', { rankInfo: { rankId: 'rank' }, dbPath: 'source.db' }]]),
        units: [{ metadata: { cardId: 'rank', dbPath: 'source.db' }, phase: 'download' }],
        unitsConfig: { offsetConfig: { timestampOffset: {} } },
    },
} as unknown as SelectContentViewProps);

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
        totalRecords: 25,
    });
});

// Keep both ResizeTable and Ant Design Table real: calling onChange directly misses the sort cancellation state.
const getHeader = (container: HTMLElement, title: string): HTMLElement => {
    const header = Array.from(container.querySelectorAll<HTMLElement>('th')).find(element =>
        (element.querySelector('.ant-table-column-title') ?? element).textContent?.replace(/\s+/g, ' ').trim() === title);
    if (header === undefined) throw new Error(`Missing table header: ${title}`);
    return header;
};

const openDetails = async (container: HTMLElement, groupId = 'matched:aten::add'): Promise<void> => {
    await waitFor(() => expect(container.querySelector('.ant-table-row-expand-icon-cell > div')).not.toBeNull());
    fireEvent.click(container.querySelector('.ant-table-row-expand-icon-cell > div') as HTMLElement);
    await waitFor(() => expect(queryFreeAttributionDetails).toHaveBeenCalledWith(expect.objectContaining({
        groupId, current: 1, orderBy: 'freeStartTime', order: 'asc',
    })));
    await waitFor(() => expect(getHeader(container, 'Free Start Time')).toHaveAttribute('aria-sort', 'ascending'));
};

const clickSort = async (
    container: HTMLElement, title: string, request: jest.Mock, field: string, order: 'asc' | 'desc',
): Promise<void> => {
    const previousCalls = request.mock.calls.length;
    fireEvent.click(getHeader(container, title));
    await waitFor(() => {
        expect(request.mock.calls.length).toBeGreaterThan(previousCalls);
        expect(request).toHaveBeenLastCalledWith(expect.objectContaining({ orderBy: field, order, current: 1 }));
        expect(getHeader(container, title)).toHaveAttribute('aria-sort', order === 'asc' ? 'ascending' : 'descending');
    });
};

it.each([
    ['Total Free Duration', 'totalFreeDuration'],
    ['Host API', 'apiName'],
    ['Total Free Interval Count', 'freeCount'],
])('keeps summary %s alternating through four header clicks', async (title, field) => {
    const { container } = render(<FreeAttribution {...makeProps()} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalledWith(expect.objectContaining({
        current: 1, orderBy: 'totalFreeDuration', order: 'desc',
    })));
    await waitFor(() => expect(getHeader(container, 'Total Free Duration')).toHaveAttribute('aria-sort', 'descending'));
    for (const order of ['asc', 'desc', 'asc', 'desc'] as const) {
        await clickSort(container, title, queryFreeAttributionSummary as jest.Mock, field, order);
    }
});

it.each([
    ['Free Start Time', 'freeStartTime', ['desc', 'asc', 'desc', 'asc']],
    ['Free Duration', 'freeDuration', ['asc', 'desc', 'asc', 'desc']],
] as const)('keeps detail %s alternating through four header clicks', async (title, field, orders) => {
    const { container } = render(<FreeAttribution {...makeProps()} />);
    await openDetails(container);
    for (const order of orders) {
        await clickSort(container, title, queryFreeAttributionDetails as jest.Mock, field, order);
    }
});

it.each([
    ['incomplete', 'Host API', 'apiName'],
    ['incomplete', 'API Start Time', 'apiStartTime'],
    ['incomplete', 'API Duration', 'apiDuration'],
    ['ambiguous', 'Host API', 'apiName'],
    ['ambiguous', 'API Start Time', 'apiStartTime'],
    ['ambiguous', 'API Duration', 'apiDuration'],
] as const)('sorts %s candidate APIs by %s through repeated clicks', async (status, title, field) => {
    const groupId = `status:${status}`;
    (queryFreeAttributionSummary as jest.Mock).mockResolvedValue({
        ...summary,
        groups: [{ ...summary.groups[0], groupId, groupType: status, apiName: '' }],
    });
    (queryFreeAttributionDetails as jest.Mock).mockResolvedValue({
        records: [{
            detailId: 'free-1',
            freeDuration: free.duration,
            freeTarget: free,
            nextBehaviorType: 'Computing',
            attributionStatus: status,
            candidateApiTargets: status === 'incomplete'
                ? [hostApi]
                : [hostApi, {
                    ...hostApi, id: 'api-2', name: 'aten::sub', timestamp: 1700, duration: 300,
                }],
        }],
        totalRecords: 1,
    });
    const { container } = render(<FreeAttribution {...makeProps()} />);
    await openDetails(container, groupId);
    const details = container.querySelector('.ant-table-expanded-row') as HTMLElement;
    await waitFor(() => expect(details).toHaveTextContent('aten::add'));
    if (status === 'ambiguous') {
        expect(details).toHaveTextContent('aten::add (2)');
        expect(details).not.toHaveTextContent('aten::sub');
    }
    for (const order of ['asc', 'desc', 'asc', 'desc'] as const) {
        await clickSort(details, title, queryFreeAttributionDetails as jest.Mock, field, order);
        expect(queryFreeAttributionDetails).toHaveBeenLastCalledWith(expect.objectContaining({ groupId }));
    }
});

it('resets the summary to page one when sorting from page two', async () => {
    const { container } = render(<FreeAttribution {...makeProps()} />);
    await waitFor(() => expect(container.querySelector('.ant-pagination-item-2')).not.toBeNull());
    fireEvent.click(container.querySelector('.ant-pagination-item-2') as HTMLElement);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({ current: 2 })));
    await clickSort(container, 'Total Free Interval Count', queryFreeAttributionSummary as jest.Mock, 'freeCount', 'asc');
    await waitFor(() => expect(container.querySelector('.ant-pagination-item-1')).toHaveClass('ant-pagination-item-active'));
});

it('resets the detail to page one when sorting from page two', async () => {
    const { container } = render(<FreeAttribution {...makeProps()} />);
    await openDetails(container);
    const details = container.querySelector('.ant-table-expanded-row') as HTMLElement;
    fireEvent.click(details.querySelector('.ant-pagination-item-2') as HTMLElement);
    await waitFor(() => expect(queryFreeAttributionDetails).toHaveBeenLastCalledWith(expect.objectContaining({ current: 2 })));
    await clickSort(container, 'Free Duration', queryFreeAttributionDetails as jest.Mock, 'freeDuration', 'asc');
    await waitFor(() => expect(details.querySelector('.ant-pagination-item-1')).toHaveClass('ant-pagination-item-active'));
});

const openNameSearch = async (view: ReturnType<typeof render>): Promise<HTMLElement> => {
    const trigger = getHeader(view.container, 'Host API').querySelector('.ant-table-filter-trigger');
    expect(trigger).not.toBeNull();
    fireEvent.click(trigger as HTMLElement);
    return await view.findByRole('textbox');
};

it('searches from page two and keeps the trimmed name in detail, sort and pagination requests', async () => {
    const view = render(<FreeAttribution {...makeProps()} />);
    await waitFor(() => expect(view.container.querySelector('.ant-pagination-item-2')).not.toBeNull());
    fireEvent.click(view.container.querySelector('.ant-pagination-item-2') as HTMLElement);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({ current: 2 })));
    const input = await openNameSearch(view);
    fireEvent.change(input, { target: { value: '  ADD  ' } });
    fireEvent.click(view.getByRole('button', { name: /^(Search|搜索)$/ }));
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({
        searchName: 'ADD', current: 1,
    })));
    await openDetails(view.container);
    expect(queryFreeAttributionDetails).toHaveBeenLastCalledWith(expect.objectContaining({ searchName: 'ADD' }));
    await clickSort(view.container, 'Total Free Interval Count', queryFreeAttributionSummary as jest.Mock, 'freeCount', 'asc');
    expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({ searchName: 'ADD' }));
    fireEvent.click(view.container.querySelector('.ant-pagination-item-2') as HTMLElement);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({
        searchName: 'ADD', current: 2, orderBy: 'freeCount', order: 'asc',
    })));
});

it.each(['reset', 'empty input'])('clears a name search through %s and returns to the first page', async method => {
    const view = render(<FreeAttribution {...makeProps()} />);
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenCalled());
    const input = await openNameSearch(view);
    fireEvent.change(input, { target: { value: 'add' } });
    fireEvent.keyDown(input, { key: 'Enter', code: 'Enter', charCode: 13, keyCode: 13 });
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({ searchName: 'add' })));
    const reopened = await openNameSearch(view);
    if (method === 'reset') {
        fireEvent.click(view.getByRole('button', { name: /^(Reset|重置)$/ }));
    } else {
        fireEvent.change(reopened, { target: { value: '' } });
        fireEvent.keyDown(reopened, { key: 'Enter', code: 'Enter', charCode: 13, keyCode: 13 });
    }
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({
        searchName: '', current: 1,
    })));
});

it('collapses old details and discards their pending response when the name search changes', async () => {
    let finishOld!: (response: unknown) => void;
    (queryFreeAttributionDetails as jest.Mock).mockImplementationOnce(() => new Promise(resolve => { finishOld = resolve; }));
    const view = render(<FreeAttribution {...makeProps()} />);
    await waitFor(() => expect(view.container.querySelector('.ant-table-row-expand-icon-cell > div')).not.toBeNull());
    fireEvent.click(view.container.querySelector('.ant-table-row-expand-icon-cell > div') as HTMLElement);
    await waitFor(() => expect(queryFreeAttributionDetails).toHaveBeenCalled());
    const input = await openNameSearch(view);
    fireEvent.change(input, { target: { value: 'mul' } });
    fireEvent.click(view.getByRole('button', { name: /^(Search|搜索)$/ }));
    await waitFor(() => expect(queryFreeAttributionSummary).toHaveBeenLastCalledWith(expect.objectContaining({ searchName: 'mul' })));
    expect(view.container.querySelector('.ant-table-expanded-row')).toBeNull();
    await act(async () => finishOld({
        records: [{
            detailId: 'old',
            freeTarget: free,
            freeDuration: 1500,
            nextBehaviorType: 'Computing',
            attributionStatus: 'matched',
            hostApiTarget: { ...hostApi, name: 'aten::stale-detail' },
            candidateApiTargets: [],
        }],
        totalRecords: 1,
    }));
    expect(view.queryByText('aten::stale-detail')).not.toBeInTheDocument();
    await openDetails(view.container);
    expect(queryFreeAttributionDetails).toHaveBeenLastCalledWith(expect.objectContaining({ searchName: 'mul' }));
});
