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
import { act, render } from '@testing-library/react';
import type { ChartProps, StatusData } from '../../entity/chart';
import { type ChartDesc, type InsightUnit, UnitHeight } from '../../entity/insight';
import type { Session } from '../../entity/session';
import { Chart } from './drawChart';
import { StatusChart } from './StatusChart';
import { useData } from './hooks';

jest.mock('./EventChart', () => ({ EventChart: (): null => null }));
jest.mock('./FilledLineChart', () => ({ FilledLineChart: (): null => null }));
jest.mock('./StackedBarChart', () => ({ StackedBarChart: (): null => null }));
jest.mock('./StackStatusChart', () => ({ StackStatusChart: (): null => null }));
jest.mock('./StatusChart', () => ({ StatusChart: jest.fn() }));
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string): string => key }) }));

// Keep the real data hook and request scheduler while omitting canvas drawing.
const StatusChartProbe = (props: ChartProps<'status'>): JSX.Element => {
    const data = useData(props);
    return <div data-testid="chart-data">{data.map(item => item.name).join(',')}</div>;
};

describe('Chart requests during resize', () => {
    it('coalesces rapid width changes and loads the final viewport', async () => {
        (StatusChart as jest.Mock).mockImplementation(StatusChartProbe);
        const session = {
            id: 'resize-test',
            phase: 'download',
            domainRange: { domainStart: 0, domainEnd: 100 },
            endTimeAll: 1000,
            units: [],
            unitsConfig: { filterConfig: {}, offsetConfig: { timestampOffset: {} } },
            threadsToFetch: new Map(),
        } as unknown as Session;
        const unit = { name: 'Label', metadata: { cardId: 'rank0' }, phase: 'download' } as unknown as InsightUnit;
        let resolveFirst!: (data: StatusData[]) => void;
        const firstRequest = new Promise<StatusData[]>(resolve => { resolveFirst = resolve; });
        const requestedEnds: number[] = [];
        const mapFunc = jest.fn((currentSession: Session) => {
            requestedEnds.push(currentSession.domainRange.domainEnd);
            return requestedEnds.length === 1
                ? firstRequest
                : Promise.resolve([{ name: 'final viewport' } as StatusData]);
        });
        const desc: ChartDesc<'status'> = {
            type: 'status', height: UnitHeight.STANDARD, mapFunc, config: { rowHeight: UnitHeight.STANDARD },
        };
        const view = (width: number): JSX.Element => <Chart
            desc={desc} serial="lane" title="lane" session={session} unit={unit}
            metadata={unit.metadata} width={width} phase="download"
        />;
        const { rerender, getByTestId } = render(view(100));

        for (const width of [220, 80, 240, 100]) {
            session.domainRange = { domainStart: 0, domainEnd: width + 10 };
            rerender(view(width));
        }
        expect(mapFunc).toHaveBeenCalledTimes(1);

        await act(async () => {
            resolveFirst([{ name: 'initial viewport' } as StatusData]);
            await firstRequest;
        });
        expect(mapFunc).toHaveBeenCalledTimes(2);
        expect(requestedEnds).toEqual([100, 110]);
        expect(getByTestId('chart-data')).toHaveTextContent('final viewport');

        // A subsequent resize must still fetch after the previous burst has finished.
        await act(async () => {
            session.domainRange = { domainStart: 0, domainEnd: 180 };
            rerender(view(180));
        });
        expect(requestedEnds).toEqual([100, 110, 180]);
    });
});
