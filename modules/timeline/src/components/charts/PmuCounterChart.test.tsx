/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React from 'react';
import { render } from '@testing-library/react';
import type { ChartProps } from '../../entity/chart';
import { FilledLineChartRenderer } from './FilledLineChart';
import { PmuCounterChart } from './PmuCounterChart';
import { usePmuCounterData } from './usePmuCounterData';
import { pmuMetadata, pmuResponse, pmuSegment } from './__test__/pmuTestUtils';

jest.mock('./FilledLineChart', () => ({
    FilledLineChartRenderer: jest.fn(({ children }) => <div>{children}</div>),
}));
jest.mock('./usePmuCounterData', () => ({ usePmuCounterData: jest.fn() }));
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('@emotion/react', () => ({ useTheme: () => ({ fontColor: '#fff' }) }));

function props(): ChartProps<'pmuCounter'> {
    return {
        session: {},
        unit: {},
        metadata: pmuMetadata(),
        width: 100,
        height: 120,
        margin: 0,
        title: 'PMU',
    } as ChartProps<'pmuCounter'>;
}

describe('PMU shared counter presentation', () => {
    beforeEach(() => jest.clearAllMocks());

    it('uses the existing metric renderer and a single metric-with-unit tooltip row', () => {
        const metadata = pmuMetadata();
        metadata.pmu.unit = 'ns';
        metadata.pmu.eventName = 'task-clock';
        (usePmuCounterData as jest.Mock).mockReturnValue({
            data: pmuResponse({ nativeUnit: 'ns', segments: [pmuSegment({ value: 2500000 })] }),
            loading: false,
            error: false,
            offset: 5,
        });
        render(<PmuCounterChart {...props()} metadata={metadata}/>);
        const renderProps = (FilledLineChartRenderer as jest.Mock).mock.calls[0][0];
        expect(renderProps.dataListState).toEqual([[5, 2.5]]);
        expect(renderProps.intervalEnds).toEqual([15]);
        expect([...renderProps.renderTooltip([5, 2.5])]).toEqual([['task-clock (ms)', '2.5']]);
        expect(renderProps.legend).toBeUndefined();
    });

    it('does not introduce per-chart loading text for retained data during zoom', () => {
        (usePmuCounterData as jest.Mock).mockReturnValue({
            data: pmuResponse(), loading: true, error: false, offset: 0,
        });
        const view = render(<PmuCounterChart {...props()}/>);
        expect(view.queryByRole('status')).toBeNull();
        expect((FilledLineChartRenderer as jest.Mock).mock.calls[0][0].dataListState).toEqual([[10, 3]]);
    });
});
