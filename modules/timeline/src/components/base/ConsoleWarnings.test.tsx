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
import { light } from '../../../../lib/src/theme/light';
import { StyledButton } from './StyledButton';
import KeyInfoTooltip from '../ChartContainer/KeyInfoTooltip';
import type { Session } from '../../entity/session';

jest.mock('@insight/lib/components', () => ({
    Tooltip: jest.requireActual('../../../../lib/src/components/MITooltip').MITooltip,
}));

jest.mock('@insight/lib/icon', () => ({
    FlagIcon: () => <div data-testid="flag-icon" />,
}));

jest.mock('react-i18next', () => ({
    useTranslation: () => ({ t: (key: string): string => key }),
}));

describe('timeline console warnings', () => {
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

    it('keeps button styling props out of the DOM while forwarding its ref and events', () => {
        const ref = React.createRef<HTMLElement>();
        const onClick = jest.fn();
        const screen = render(<ThemeProvider theme={light}>
            <StyledButton ref={ref} transparent width={36} onClick={onClick}>Action</StyledButton>
        </ThemeProvider>);
        const button = screen.getByRole('button', { name: 'Action' });
        expect(ref.current).toBe(button);
        act(() => ref.current?.focus());
        expect(document.activeElement).toBe(button);
        expect(button.hasAttribute('transparent')).toBe(false);
        expect(button.hasAttribute('width')).toBe(false);
        expect(getComputedStyle(button).width).toBe('36px');
        fireEvent.click(button);
        expect(onClick).toHaveBeenCalledTimes(1);

        screen.rerender(<ThemeProvider theme={light}>
            <StyledButton ref={ref} transparent={false} disabled onClick={onClick}>Action</StyledButton>
        </ThemeProvider>);
        fireEvent.click(button);
        expect(onClick).toHaveBeenCalledTimes(1);
    });

    it('forwards a link button ref without changing navigation attributes', () => {
        const ref = React.createRef<HTMLElement>();
        const screen = render(<ThemeProvider theme={light}>
            <StyledButton ref={ref} href="#target" transparent>Link</StyledButton>
        </ThemeProvider>);
        expect(ref.current).toBe(screen.getByRole('link', { name: 'Link' }));
        expect(ref.current?.getAttribute('href')).toBe('#target');
    });

    it('renders the marker icon without invalid paragraph nesting', () => {
        const session = { showCreateFlagMarkKey: true } as Session;
        const screen = render(<ThemeProvider theme={light}><KeyInfoTooltip session={session} /></ThemeProvider>);
        expect(screen.getByTestId('flag-icon').closest('p')).toBeNull();
        expect(screen.getByText('K')).toBeTruthy();
    });
});
