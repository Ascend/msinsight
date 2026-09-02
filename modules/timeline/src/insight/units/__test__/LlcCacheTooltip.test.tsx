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
import { ThemeProvider, type Theme } from '@emotion/react';
import { render } from '@testing-library/react';
import { LlcCacheTooltip } from '../LlcCacheTooltip';
import type { LlcCacheStackedBarData } from '../llcCache';

const data: LlcCacheStackedBarData = {
    timestamp: 13_320_000_000,
    bucketWidthNs: 180_000_000,
    values: [100, 900],
    hits: 900,
    misses: 100,
    totalAccesses: 1000,
    hitRate: 90,
    missRate: 10,
};

describe('LlcCacheTooltip', () => {
    it('shows bucket timing, counts, rates and total accesses', () => {
        const { getByText } = render(<LlcCacheTooltip data={data}/>);

        expect(getByText('13.32s - 13.50s')).toBeInTheDocument();
        expect(getByText(/Aggregation Interval: 0.18s/)).toBeInTheDocument();
        expect(getByText(/Total Accesses: 1,000/)).toBeInTheDocument();
        expect(getByText('LLC Hits')).toBeInTheDocument();
        expect(getByText('LLC Misses')).toBeInTheDocument();
        expect(getByText('90.0%')).toBeInTheDocument();
        expect(getByText('10.0%')).toBeInTheDocument();
        const metricOrder = getByText('LLC Misses')
            .compareDocumentPosition(getByText('LLC Hits'));
        expect(metricOrder & Node.DOCUMENT_POSITION_FOLLOWING).toBe(Node.DOCUMENT_POSITION_FOLLOWING);
    });

    it('uses the active theme for text and proportion tracks', () => {
        const theme = {
            tooltipFontColor: '#123456',
            textColorSecondary: '#234567',
            borderColorLight: '#345678',
        } as Theme;
        const { getByText } = render(<ThemeProvider theme={theme}><LlcCacheTooltip data={data}/></ThemeProvider>);

        expect(getByText('13.32s - 13.50s').parentElement).toHaveStyle('color: #123456');
        expect(getByText(/Aggregation Interval: 0.18s/)).toHaveStyle('color: #234567');
        expect(document.querySelector('[aria-hidden="true"]')).toHaveStyle('background: #345678');
    });

    it('formats sub-centisecond aggregation intervals with two decimal places', () => {
        const { getByText, queryByText } = render(<LlcCacheTooltip data={{ ...data, bucketWidthNs: 1_000_000 }}/>);

        expect(getByText(/Aggregation Interval: 0.00s/)).toBeInTheDocument();
        expect(queryByText(/0.001000s/)).not.toBeInTheDocument();
    });
});
