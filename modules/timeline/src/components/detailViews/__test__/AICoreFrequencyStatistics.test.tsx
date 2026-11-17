/* Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React from 'react';
import { observable, runInAction } from 'mobx';
import i18next from 'i18next';
import { useTranslation } from 'react-i18next';
// eslint-disable-next-line import/named -- 由 @testing-library/dom 重新导出。
import { act, render, waitFor } from '@testing-library/react';
import { ResizeTable } from '@insight/lib/resize';
import { queryExpertAnalysis } from '../../../api/request';
import { AICoreFrequencyStatistics, collectFrequencyLanes } from '../AICoreFrequencyStatistics';
import type { SelectContentViewProps } from '../SystemView';

jest.mock('react-i18next', () => ({ useTranslation: jest.fn() }));
jest.mock('@insight/lib/resize', () => ({ ResizeTable: jest.fn(() => null), fetchColumnFilterProps: () => ({}) }));
jest.mock('@insight/lib/components', () => ({ Tooltip: ({ title, children }: any) => <span title={title}>{children}</span> }));
jest.mock('../../../api/request', () => ({ queryExpertAnalysis: jest.fn() }));
jest.mock('../../../insight/units/utils', () => jest.requireActual('../../../insight/units/offset'));
jest.mock('../SystemView', () => ({ DETAIL_HEADER_HEIGHT_ETC_PX: 120 }));

const result = { hasProblem: false, percent: 0, avgFreq: 1250.25, sampleCount: 4, minFreq: 1000, maxFreq: 1500 };
const props = (range?: [number, number], count = 1): SelectContentViewProps => observable({
    card: { cardId: 'rank', dbPath: 'test.db' },
    bottomHeight: 600,
    session: {
        units: [{
            name: 'Card',
            metadata: { cardId: 'rank' },
            phase: 'download',
            children: Array.from({ length: count }, (_, i) => ({
                metadata: {
                    cardId: 'rank',
                    dbPath: 'test.db',
                    threadName: 'AI Core Freq',
                    processName: `Device ${i}`,
                    processId: String(32 + i),
                    metaType: i === 1 ? 'TEXT' : 'NPU_METRICS',
                    offsetSide: i === 1 ? 'device' : undefined,
                },
            })),
        }],
        selectedUnits: [],
        timeAnalysisRange: range,
        domain: { chartViewWidth: 100 },
        domainRange: { domainStart: 0, domainEnd: 1050 },
        endTimeAll: 1050,
        unitsConfig: { offsetConfig: { timestampOffset: { rank__host: 0, rank__device: 20.5 } } },
    },
} as unknown as SelectContentViewProps);
const table = (): any => (ResizeTable as jest.Mock).mock.calls.slice(-1)[0][0];
const requests = (): any[] => (queryExpertAnalysis as jest.Mock).mock.calls.map(([p]) => p);
const column = (key: string): any => table().columns.find((c: any) => (c.key ?? c.dataIndex) === key);
const loaded = async (p = props()): Promise<void> => {
    render(<AICoreFrequencyStatistics {...p} />);
    await waitFor(() => expect(table().loading).toBe(false));
};
beforeEach(() => {
    jest.clearAllMocks();
    (useTranslation as jest.Mock).mockReturnValue({ t: (key: string) => key });
    (queryExpertAnalysis as jest.Mock).mockResolvedValue(result);
});

test('full-data table keeps five sortable/filterable columns and localized pagination', async () => {
    await loaded(props(undefined, 3));
    expect(requests().map(p => p.lanePid)).toEqual(['32', '33', '34']);
    expect(requests()[0]).toMatchObject({ hasTimeRange: false, startTime: 0, endTime: 0, counterStartTime: 20, counterEndTime: 1071 });
    const row = table().dataSource[0];
    expect(table().columns.map((c: any) => c.dataIndex ?? c.key)).toEqual(['parentLane', 'laneName', 'avgFreq', 'minFreq', 'maxFreq']);
    expect(column('parentLane').onFilter('device', row)).toBe(true);
    expect(column('parentLane').onFilter('other', row)).toBe(false);
    expect(column('laneName').onFilter('freq', row)).toBe(true);
    expect(column('parentLane').sorter(row, { ...row, parentLane: 'Z' })).toBeLessThan(0);
    for (const field of ['avgFreq', 'minFreq', 'maxFreq']) {
        expect(column(field).sorter(row, { ...row, result: { ...result, [field]: 9000 } })).toBeLessThan(0);
    }
    expect(column('avgFreq').render(undefined, row)).toBe('1250.25');
    const translator = i18next.createInstance();
    await translator.init({ lng: 'zh', resources: { zh: jest.requireActual('../../../../../lib/src/i18n/zh.json'), en: jest.requireActual('../../../../../lib/src/i18n/en.json') } });
    (useTranslation as jest.Mock).mockReturnValue({ t: translator.getFixedT(null, 'translation') });
    await loaded();
    expect(table().pagination.showTotal(3)).toBe('共 3 条');
    expect(table().pagination.showTotal(0)).toBe('共 0 条');
    const calls = requests().length;
    await translator.changeLanguage('en');
    expect(table().pagination.showTotal(3)).toBe('Total 3 items');
    expect(table().pagination.showTotal(0)).toBe('Total 0 items');
    expect(requests()).toHaveLength(calls);
});

test('range analysis observes each lane side, Device alignment and actual pixel geometry', async () => {
    const p = props(undefined, 3);
    const children = p.session.units[0].children as typeof p.session.units;
    children[2].metadata.offsetSide = 'host';
    await loaded(p);
    act(() => runInAction(() => { p.session.selectedRange = [500, 600]; }));
    expect(requests()).toHaveLength(3);
    act(() => runInAction(() => {
        p.session.timeAnalysisRange = [10, 20]; p.session.unitsConfig.offsetConfig.timestampOffset.rank__device = 100;
    }));
    await waitFor(() => expect(requests().slice(-3).map(r => r.startTime)).toEqual([110, 110, 10]));
    expect(requests().slice(-3)[1]).toMatchObject({ endTime: 120, viewStartTime: 100, viewEndTime: 1150, counterStartTime: 100, counterEndTime: 1150 });
    act(() => runInAction(() => {
        p.session.unitsConfig.offsetConfig.timestampOffset.rank__device = 200;
        (p.session.domainRange as { domainStart: number }).domainStart = 100; children[0].counterRenderWidth = 200;
    }));
    await waitFor(() => expect(requests().slice(-3)[0]).toMatchObject({ startTime: 210, endTime: 220, viewStartTime: 300, chartWidth: 200 }));
    act(() => runInAction(() => { p.session.timeAnalysisRange = undefined; }));
    await waitFor(() => expect(requests().slice(-3).map(r => [r.startTime, r.endTime, r.counterStartTime, r.counterEndTime]))
        .toEqual([[0, 0, 200, 1250], [0, 0, 200, 1250], [0, 0, 0, 1050]]));
});

test('switching the selected card/source excludes other rows and preserves aliases/loading state', async () => {
    const p = props([10, 20]);
    const root = p.session.units[0]; const child = root.children?.[0] as typeof root;
    child.parent = root;
    const second = {
        ...root,
        metadata: { ...root.metadata, cardId: 'rank2', cardName: 'Card 2' },
        children: ['second.db', 'extra.db'].map(dbPath => ({
            ...child, parent: undefined, metadata: { ...child.metadata, cardId: 'rank2', dbPath },
        })),
    } as typeof root;
    p.session.units = [child, root, second];
    p.session.unitsConfig.offsetConfig.timestampOffset.rank2__device = 100;
    (queryExpertAnalysis as jest.Mock).mockImplementation(({ dbPath }) => Promise.resolve({
        ...result, avgFreq: dbPath === 'test.db' ? 1250 : dbPath === 'extra.db' ? 700 : 1800,
    }));
    await loaded(p);
    expect(collectFrequencyLanes(p.session.units, p.card)).toHaveLength(1);
    expect(requests()).toHaveLength(1);
    expect(table().dataSource[0].result.avgFreq).toBe(1250);
    act(() => runInAction(() => { p.card.cardId = 'rank2'; p.card.dbPath = 'second.db'; }));
    await waitFor(() => expect(table().dataSource[0]?.result.avgFreq).toBe(1800));
    expect(requests().slice(-1)[0]).toMatchObject({ rankId: 'rank2', dbPath: 'second.db', startTime: 110, endTime: 120 });
    expect(table().dataSource).toHaveLength(1);
    act(() => runInAction(() => { p.card.dbPath = 'extra.db'; }));
    await waitFor(() => expect(table().dataSource[0]?.result.avgFreq).toBe(700));
    expect(requests().slice(-1)[0]).toMatchObject({ dbPath: 'extra.db', counterStartTime: 100, counterEndTime: 1150 });
    act(() => runInAction(() => { p.session.units[2].phase = 'loading'; }));
    await waitFor(() => expect(table().dataSource).toHaveLength(0));
    act(() => runInAction(() => { p.session.units[2].phase = 'download'; }));
    await waitFor(() => expect(table().dataSource).toHaveLength(1));
    act(() => runInAction(() => {
        p.card.dbPath = '';
        const lane = p.session.units[2].children?.[1];
        if (lane) { lane.metadata.dbPath = ''; }
    }));
    await waitFor(() => expect(requests().slice(-1)[0].dbPath).toBe('')); // 空路径仍可按当前卡路由。
});

test('empty/failed lanes do not discard successful rows and stale responses cannot overwrite them', async () => {
    let resolveOld: (v: typeof result) => void = () => {};
    (queryExpertAnalysis as jest.Mock).mockImplementationOnce(() => new Promise(resolve => { resolveOld = resolve; }));
    const view = render(<AICoreFrequencyStatistics {...props([100, 200])} />);
    await waitFor(() => expect(requests()).toHaveLength(1));
    (queryExpertAnalysis as jest.Mock).mockImplementation(({ lanePid }) => lanePid === '34'
        ? Promise.reject(new Error('query'))
        : Promise.resolve(lanePid === '33' ? { ...result, sampleCount: 0 } : result));
    const next = props([300, 400], 3);
    next.card.cardId = 'rank2'; next.session.units[0].metadata.cardId = 'rank2';
    next.session.units[0].children?.forEach(unit => { unit.metadata.cardId = 'rank2'; });
    view.rerender(<AICoreFrequencyStatistics {...next} />);
    await waitFor(() => expect(table().dataSource).toHaveLength(3));
    const rows = table().dataSource;
    expect(rows.every((row: any) => row.cardId === 'rank2')).toBe(true);
    expect(column('avgFreq').render(undefined, rows[0])).toBe('1250.25');
    expect(column('avgFreq').render(undefined, rows[1])).toBe('-');
    expect(column('avgFreq').render(undefined, rows[2]).props.title).toBe('AI Core Frequency Query Failed');
    await act(async () => { resolveOld({ ...result, avgFreq: 9999 }); });
    expect(table().dataSource).toEqual(rows);
});

test('limits concurrency and stops queued requests on unmount', async () => {
    const resolvers: Array<(v: typeof result) => void> = [];
    (queryExpertAnalysis as jest.Mock).mockImplementation(() => new Promise(resolve => { resolvers.push(resolve); }));
    const view = render(<AICoreFrequencyStatistics {...props(undefined, 5)} />);
    await waitFor(() => expect(requests()).toHaveLength(4));
    view.unmount();
    await act(async () => { resolvers.forEach(resolve => resolve(result)); });
    expect(requests()).toHaveLength(4);
});
