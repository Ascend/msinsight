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
import { fireEvent, render } from '@testing-library/react';
import * as testingLibrary from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import { createInstance } from 'i18next';
import { I18nextProvider, initReactI18next } from 'react-i18next';
import en from '../../../lib/src/i18n/leaks/en.json';
import zh from '../../../lib/src/i18n/leaks/zh.json';
import { LifecycleGraphToolbar } from '../common/components/leaks/LifecycleGraphToolbar';
import { FrameTreeDetails } from '../frameTree/FrameTreeDetails';
import { loadFrameTree } from '../frameTree/loader';
import { createMockFrameTreePage } from './fixtures/frameTreeMock';

const { screen, waitFor, within } = testingLibrary;
jest.mock('@insight/lib/components', () => {
    const { Tooltip, Button } = jest.requireActual('antd');
    return { Tooltip, Button };
});
const translations = createInstance() as React.ComponentProps<typeof I18nextProvider>['i18n'];
const theme = { primaryColor: '#0074d9', borderColor: '#ccc', borderColorLight: '#ddd', textColorPrimary: '#111' } as any;
const wrap = (children: React.ReactNode): JSX.Element =>
    <I18nextProvider i18n={translations}><ThemeProvider theme={theme}>{children}</ThemeProvider></I18nextProvider>;
beforeAll(async () => {
    await translations.use(initReactI18next).init({
        lng: 'enUS', fallbackLng: 'enUS', resources: { enUS: en, zhCN: zh }, interpolation: { escapeValue: false },
    });
});

it('shows a bilingual breakdown illustration only when time markers are supported', async () => {
    const toolbar = (supported: boolean): JSX.Element => wrap(<LifecycleGraphToolbar zoomMode="proportional"
        onZoomModeChange={jest.fn()} onReset={jest.fn()} onTimeMarkerManagementOpen={supported ? jest.fn() : undefined} />);
    const view = render(toolbar(true));
    fireEvent.click(screen.getByRole('button', { name: en.leaks.lifecycleGraphGuideTitle }));
    const guide = screen.getByRole('dialog', { name: en.leaks.lifecycleGraphGuideTitle });
    const artwork = within(guide).getByTestId('lifecycleGuideBreakdownManagementVisual');
    expect(artwork.querySelector('svg')).toHaveAttribute('aria-hidden', 'true');
    expect(within(artwork).getAllByText(/^Event ID /)).toHaveLength(2);
    for (const [language, name, count] of [['enUS', 'Event 183240', 'Breakdowns: 2'], ['zhCN', '\u4e8b\u4ef6 183240', '\u5171 2 \u4e2a\u62c6\u89e3']]) {
        await testingLibrary.act(async () => { await translations.changeLanguage(language); });
        expect(within(artwork).getByText(name)).toBeTruthy();
        expect(within(artwork).getByText(count)).toBeTruthy();
    }
    view.rerender(toolbar(false));
    expect(within(guide).queryByTestId('lifecycleGuideBreakdownManagementVisual')).toBeNull();
});

it('keeps source-path hover and focus help without adding metric or ratio tooltips', async () => {
    const filename = '/home/user/miniconda3/envs/torch-2.6-py311/lib/python3.11/site-packages/torchvision/models/detection/faster_rcnn.py';
    const tree = await loadFrameTree({ deviceId: '0', eventId: 204412, includeStatic: true, pageSize: 1000 },
        createMockFrameTreePage, new AbortController().signal);
    const node = tree.nodes.get('branch-1');
    if (!node) throw new Error('Missing forward fixture node');
    node.dto.frame = { filename, line: 123, name: 'forward' };
    const view = render(wrap(<FrameTreeDetails tree={tree} selected="branch-1" locked onSelect={jest.fn()} onEnter={jest.fn()} />));
    expect(view.container.querySelectorAll('.frame-tree-inspector-metrics [tabindex], .frame-tree-inspector-ratios [tabindex]')).toHaveLength(0);
    const source = view.container.querySelector('.frame-tree-inspector-source') as HTMLElement;
    expect(source.textContent).toBe(`${filename}:123`);
    expect(source).toHaveAttribute('tabindex', '0');
    fireEvent.mouseEnter(source);
    const tooltip = await screen.findByRole('tooltip');
    expect(tooltip.textContent).toBe(`${filename}:123`);
    fireEvent.mouseLeave(source);
    await waitFor(() => expect(tooltip.closest('.ant-tooltip')).toHaveClass('ant-tooltip-hidden'));
    fireEvent.focus(source);
    await waitFor(() => expect(tooltip.closest('.ant-tooltip')).not.toHaveClass('ant-tooltip-hidden'));
    fireEvent.blur(source);
});
