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
// eslint-disable-next-line import/named
import { render, screen } from '@testing-library/react';
import { ThreadingTooltip } from '../ThreadingTooltip';
import type { ThreadingStackedBarData } from '../threadingAnalysis';

const data: ThreadingStackedBarData = {
    timestamp: 13_320_000_000,
    bucketWidthNs: 180_000_000,
    values: [40, 13, 37, 10],
    stateSeconds: [0.08, 0.026, 0.074, 0.02],
};

describe('ThreadingTooltip', () => {
    it('matches the documented time range and state detail hierarchy', () => {
        render(<ThreadingTooltip data={data}/>);

        expect(screen.getByText('13.32s - 13.50s')).toBeInTheDocument();
        expect(screen.getByText('Aggregation Interval: 0.18s')).toBeInTheDocument();
        expect(screen.getByText('Active (CPU) time')).toBeInTheDocument();
        expect(screen.getByText('0.08s')).toBeInTheDocument();
        expect(screen.getByText('40.0%')).toBeInTheDocument();
        expect(screen.getByText('Sync Wait')).toBeInTheDocument();
        expect(screen.getByText('0.03s')).toBeInTheDocument();
        expect(screen.queryByText('0.026s')).not.toBeInTheDocument();
        expect(screen.getByText('Preemption')).toBeInTheDocument();
        expect(screen.getByText('Unknown')).toBeInTheDocument();
        expect(screen.queryByText('Thread coverage')).not.toBeInTheDocument();
    });

    it('uses the active theme for text and proportion tracks', () => {
        const theme = {
            tooltipFontColor: '#123456',
            textColorSecondary: '#234567',
            borderColorLight: '#345678',
        } as Theme;
        render(<ThemeProvider theme={theme}><ThreadingTooltip data={data}/></ThemeProvider>);

        expect(screen.getByText('13.32s - 13.50s').parentElement).toHaveStyle('color: #123456');
        expect(screen.getByText('Aggregation Interval: 0.18s')).toHaveStyle('color: #234567');
        expect(document.querySelector('[aria-hidden="true"]')).toHaveStyle('background: #345678');
    });
});
