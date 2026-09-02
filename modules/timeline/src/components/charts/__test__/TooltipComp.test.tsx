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

import { ThemeProvider, type Theme } from '@emotion/react';
import { render, screen } from '@testing-library/react';
import React from 'react';
import type { Session } from '../../../entity/session';
import { TooltipComponent } from '../TooltipComp';

const CHARTINTERACTOR_NAME = 'chartInteractor';

jest.mock('../../ChartContainer/ChartContainer', () => ({ CHARTINTERACTOR_NAME: 'chartInteractor' }));
jest.mock('../ChartInteractor/draw', () => ({ PAGE_PADDING: 0 }));

const createTheme = (background: string, foreground: string, border: string): Theme => ({
    tooltipBGColor: background,
    tooltipFontColor: foreground,
    borderColorLight: border,
    tooltipBoxShadow: 'none',
} as Theme);

describe('TooltipComponent theme', () => {
    beforeEach(() => {
        const portal = document.createElement('div');
        portal.id = CHARTINTERACTOR_NAME;
        document.body.appendChild(portal);
    });

    afterEach(() => {
        document.getElementById(CHARTINTERACTOR_NAME)?.remove();
    });

    it('updates a custom tooltip when the application theme changes', () => {
        const session = { domainRange: { domainStart: 0, domainEnd: 10 } } as Session;
        const dom = { current: document.createElement('div') } as React.RefObject<HTMLDivElement>;
        const renderTooltip = (theme: Theme): JSX.Element => <ThemeProvider theme={theme}>
            <TooltipComponent
                data={1}
                session={session}
                x={() => 10}
                mouseX={10}
                calcHeight={() => 10}
                dataset={[1]}
                dom={dom}
                renderContent={() => <span>Custom tooltip</span>}
            />
        </ThemeProvider>;
        const { rerender } = render(renderTooltip(createTheme('#ffffff', '#112233', '#ddeeff')));

        const tooltip = screen.getByRole('tooltip');
        expect(tooltip).toHaveStyle('background-color: #ffffff');
        expect(tooltip).toHaveStyle('color: #112233');
        expect(tooltip).toHaveStyle('border: 1px solid #ddeeff');

        rerender(renderTooltip(createTheme('#272c34', '#f8fafc', '#3e4551')));
        expect(tooltip).toHaveStyle('background-color: #272c34');
        expect(tooltip).toHaveStyle('color: #f8fafc');
        expect(tooltip).toHaveStyle('border: 1px solid #3e4551');
    });
});
