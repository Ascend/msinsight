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
import { act, cleanup, fireEvent, render } from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import { Popconfirm } from 'antd';
import type { BaseSelectRef } from 'rc-select/lib/BaseSelect';
import type RcTree from 'rc-tree';
import { light } from '../theme/light';
import { MIInputNumber } from './MIInput';
import { MISelect } from './MISelect';
import { MISwitch } from './MISwitch';
import { MITree } from './MITree';
import { MITooltip, MITooltipHelp } from './MITooltip';
import { MIButton } from './MIButton';
import { Hit } from '../utils/Common';

jest.mock('react-i18next', () => ({
    useTranslation: () => ({ t: (key: string): string => key }),
}));

const renderWithTheme = (element: React.ReactElement): ReturnType<typeof render> => render(
    <ThemeProvider theme={light}>{element}</ThemeProvider>,
);

describe('component refs and console warnings', () => {
    let errorSpy: jest.SpyInstance;

    beforeEach(() => {
        errorSpy = jest.spyOn(console, 'error').mockImplementation(() => {});
    });

    afterEach(() => {
        cleanup();
        const errors = errorSpy.mock.calls;
        errorSpy.mockRestore();
        expect(errors).toEqual([]);
    });

    it('forwards the numeric input ref and keeps styling props out of the DOM', () => {
        const ref = React.createRef<HTMLInputElement>();
        const screen = renderWithTheme(<MIInputNumber ref={ref} center defaultValue={12} />);
        expect(ref.current).toBe(screen.getByRole('spinbutton'));
        act(() => ref.current?.focus());
        expect(document.activeElement).toBe(ref.current);
        expect(ref.current?.hasAttribute('center')).toBe(false);
        expect(getComputedStyle(screen.getByRole('spinbutton')).textAlign).toBe('center');
    });

    it.each([false, true])('forwards the select ref and preserves disabled options (custom render: %s)', (customRender) => {
        const ref = React.createRef<BaseSelectRef>();
        const onChange = jest.fn();
        const screen = renderWithTheme(<MISelect
            ref={ref}
            open
            virtual={false}
            options={[{ value: 'disabled', label: 'Disabled', disabled: true }, { value: 'enabled', label: 'Enabled' }]}
            optionRender={customRender ? (option) => <span>{option.label}</span> : undefined}
            onChange={onChange}
        />);
        expect(ref.current).not.toBeNull();
        act(() => ref.current?.focus());
        expect(document.activeElement).toBe(screen.getByRole('combobox'));
        fireEvent.click(screen.getByText('Disabled'));
        expect(onChange).not.toHaveBeenCalled();
        fireEvent.click(screen.getByText('Enabled'));
        expect(onChange).toHaveBeenCalledWith('enabled', expect.anything());
    });

    it('forwards the switch ref without losing click behavior', () => {
        const ref = React.createRef<React.ComponentRef<typeof MISwitch>>();
        const onChange = jest.fn();
        const screen = renderWithTheme(<MISwitch ref={ref} onChange={onChange} />);
        expect(ref.current).toBe(screen.getByRole('switch'));
        act(() => ref.current?.focus());
        expect(document.activeElement).toBe(ref.current);
        fireEvent.click(screen.getByRole('switch'));
        expect(onChange).toHaveBeenCalledWith(true, expect.anything());
    });

    it('supports a button ref and confirmation trigger', () => {
        const ref = React.createRef<React.ComponentRef<typeof MIButton>>();
        const onConfirm = jest.fn();
        const screen = renderWithTheme(<Popconfirm title="Proceed?" onConfirm={onConfirm}>
            <MIButton ref={ref}>Generate</MIButton>
        </Popconfirm>);
        act(() => ref.current?.focus());
        expect(document.activeElement).toBe(screen.getByRole('button', { name: 'Generate' }));
        fireEvent.click(screen.getByRole('button', { name: 'Generate' }));
        fireEvent.click(screen.getByRole('button', { name: 'OK' }));
        expect(onConfirm).toHaveBeenCalledTimes(1);
    });

    it('renders repeated advice lines without key or selector warnings', () => {
        const screen = renderWithTheme(<Hit text={'Advice\nAdvice\r\nNext'} />);
        expect(screen.container.querySelectorAll('br')).toHaveLength(3);
        expect(screen.container.textContent).toContain('Advice Advice Next');
    });

    it('exposes the tree instance through the wrapper', () => {
        const ref = React.createRef<RcTree>();
        const screen = renderWithTheme(<MITree ref={ref} treeData={[{ key: 'root', title: 'Root' }]} />);
        expect(ref.current).not.toBeNull();
        expect(typeof ref.current?.scrollTo).toBe('function');
        expect(screen.getByText('Root')).toBeTruthy();
    });

    it('forwards tooltip refs to the underlying tooltip', () => {
        const tooltipRef = React.createRef<React.ComponentRef<typeof MITooltip>>();
        const helpRef = React.createRef<React.ComponentRef<typeof MITooltipHelp>>();
        renderWithTheme(<>
            <MITooltip ref={tooltipRef} title="Hint"><button>Target</button></MITooltip>
            <MITooltipHelp ref={helpRef} title="Help" />
        </>);
        expect(tooltipRef.current).not.toBeNull();
        expect(helpRef.current).not.toBeNull();
    });
});
