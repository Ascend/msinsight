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
import { ThemeProvider } from '@emotion/react';
import { act, fireEvent, render, screen } from '@testing-library/react';
import { runInAction } from 'mobx';
import React from 'react';
import FloatingToolbar from './FloatingToolbar';
import { Session } from '../../entity/session';

jest.mock('react-i18next', () => ({
    useTranslation: (): { t: (key: string, options?: Record<string, string>) => string } => ({
        t: (key: string, options?: Record<string, string>): string => options?.key === undefined ? key : `${key}(${options.key})`,
    }),
}));

jest.mock('@insight/lib/utils', () => ({
    getShortcutKey: (): string => 'Ctrl',
}));

jest.mock('@insight/lib/components', () => ({
    Tooltip: ({ children }: { children: React.ReactNode }): JSX.Element => <>{children}</>,
}));

jest.mock('../../assets/images/floating-tools/arrow.svg', () => ({ ReactComponent: () => <svg data-testid="arrow-icon" /> }));
jest.mock('../../assets/images/floating-tools/chevron-double-right.svg', () => ({ ReactComponent: () => <svg data-testid="toggle-icon" /> }));
jest.mock('../../assets/images/floating-tools/height.svg', () => ({ ReactComponent: () => <svg data-testid="height-icon" /> }));
jest.mock('../../assets/images/floating-tools/help.svg', () => ({ ReactComponent: () => <svg data-testid="help-icon" /> }));
jest.mock('../../assets/images/floating-tools/move.svg', () => ({ ReactComponent: () => <svg data-testid="move-icon" /> }));
jest.mock('../../assets/images/floating-tools/slice.svg', () => ({ ReactComponent: () => <svg data-testid="slice-icon" /> }));
jest.mock('../../assets/images/floating-tools/sample-1.png', () => 'sample-default.png');
jest.mock('../../assets/images/floating-tools/sample-2.png', () => 'sample-height.png');
jest.mock('../../assets/images/floating-tools/sample-3.png', () => 'sample-slice.png');
jest.mock('../../assets/images/floating-tools/sample-4.png', () => 'sample-drag.png');

const theme = {
    borderColor: '#d9d9d9',
    borderColorLighter: '#eeeeee',
    contentBackgroundColor: '#ffffff',
    boxShadow: 'none',
    textColorDisabled: '#999999',
    iconColor: '#222222',
    selectedChartBackgroundColor: '#acc3f5',
    bgColorLight: '#f4f6fa',
    bgColor: '#ffffff',
    textColorPrimary: '#111111',
    textColorSecondary: '#666666',
    primaryColor: '#3478f6',
};

const renderToolbar = (session: Session): ReturnType<typeof render> => render(
    <ThemeProvider theme={theme}>
        <FloatingToolbar session={session} />
    </ThemeProvider>,
);

const getButton = (testId: string): HTMLElement => screen.getByTestId(testId).closest('button') as HTMLElement;

describe('FloatingToolbar', () => {
    // 验证工具栏点击可在默认、平移和框选三种互斥鼠标模式之间切换。
    it('switches between default, drag and selection modes from toolbar clicks', () => {
        const session = new Session();
        renderToolbar(session);

        fireEvent.click(getButton('timeline-floating-toolbar-drag-mode'));
        expect(session.panMode).toBe(true);
        expect(session.sliceSelection.active).toBe(false);

        fireEvent.click(getButton('timeline-floating-toolbar-selection-mode'));
        expect(session.panMode).toBe(false);
        expect(session.sliceSelection.active).toBe(true);
        expect(session.sliceSelection.activeIsChanged).toBe(true);

        fireEvent.click(getButton('timeline-floating-toolbar-default-mode'));
        expect(session.panMode).toBe(false);
        expect(session.sliceSelection.active).toBe(false);
    });

    // 验证泳道高度自适应按钮独立生效，不影响当前鼠标模式。
    it('keeps auto fit height independent from mouse modes', () => {
        const session = new Session();
        session.sliceSelection.active = true;
        renderToolbar(session);

        fireEvent.click(getButton('timeline-floating-toolbar-auto-fit-height'));
        expect(session.autoAdjustUnitHeight).toBe(true);
        expect(session.sliceSelection.active).toBe(true);

        fireEvent.click(getButton('timeline-floating-toolbar-auto-fit-height'));
        expect(session.autoAdjustUnitHeight).toBe(false);
        expect(session.sliceSelection.active).toBe(true);
    });

    // 验证按住快捷键时仅临时高亮平移模式，松开后恢复原持久模式。
    it('uses temporary pan visual mode while preserving persistent selection mode', () => {
        const session = new Session();
        session.sliceSelection.active = true;
        session.panModePressed = true;
        const { rerender } = renderToolbar(session);

        expect(getButton('timeline-floating-toolbar-drag-mode')).toHaveStyle('background-color: rgb(172, 195, 245)');
        expect(getButton('timeline-floating-toolbar-selection-mode')).toHaveStyle('background-color: transparent');
        expect(session.sliceSelection.active).toBe(true);

        act(() => {
            runInAction(() => {
                session.panModePressed = false;
            });
        });
        rerender(
            <ThemeProvider theme={theme}>
                <FloatingToolbar session={session} />
            </ThemeProvider>,
        );
        expect(getButton('timeline-floating-toolbar-selection-mode')).toHaveStyle('background-color: rgb(172, 195, 245)');
    });

    // 验证时间分析模式下框选按钮禁用，点击后不会开启框选状态。
    it('disables selection mode in time analysis mode', () => {
        const session = new Session();
        session.isTimeAnalysisMode = true;
        renderToolbar(session);

        const selectionButton = getButton('timeline-floating-toolbar-selection-mode') as HTMLButtonElement;
        expect(selectionButton.disabled).toBe(true);

        fireEvent.click(selectionButton);
        expect(session.sliceSelection.active).toBe(false);
    });

    // 验证帮助面板可打开，并在工具栏收起时自动关闭。
    it('shows guide panel and closes it when toolbar collapses', () => {
        const session = new Session();
        renderToolbar(session);

        fireEvent.click(getButton('timeline-floating-toolbar-guide'));
        expect(screen.getByText('floatingToolbar.guideTitle')).toBeInTheDocument();
        expect(screen.getAllByRole('img')).toHaveLength(4);

        fireEvent.click(getButton('timeline-floating-toolbar-toggle'));
        expect(screen.queryByText('floatingToolbar.guideTitle')).not.toBeInTheDocument();
    });
});
