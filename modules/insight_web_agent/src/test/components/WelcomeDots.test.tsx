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
import React, { useRef } from 'react';
import { render } from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import { themeInstance } from '@insight/lib/theme';
import { WelcomeDots } from '../../components/WelcomeDots';
import { createWelcomeDotsEffect } from '../../components/welcomeDotsEffect';

jest.mock('../../components/welcomeDotsEffect', () => ({ createWelcomeDotsEffect: jest.fn() }));

const Harness = ({ replayKey, dark = false }: { replayKey: string; dark?: boolean }): JSX.Element => {
    const ref = useRef<HTMLDivElement>(null);
    return <ThemeProvider theme={themeInstance.getTheme()[dark ? 'dark' : 'light']}>
        <div ref={ref}><WelcomeDots hitAreaRef={ref} replayKey={replayKey} /></div>
    </ThemeProvider>;
};

test('replays for a new welcome request, updates the theme without recreation and disposes on unmount', () => {
    const effect = { ripple: jest.fn(), setDarkMode: jest.fn(), destroy: jest.fn() };
    (createWelcomeDotsEffect as jest.Mock).mockReturnValue(effect);
    const { rerender, unmount, container } = render(<Harness replayKey="draft:0" />);
    expect(container.querySelector('canvas')?.getAttribute('aria-hidden')).toBe('true');
    expect(effect.ripple).toHaveBeenCalledTimes(1);
    rerender(<Harness replayKey="draft:0" dark />);
    expect(effect.setDarkMode).toHaveBeenLastCalledWith(true);
    expect(effect.ripple).toHaveBeenCalledTimes(1);
    rerender(<Harness replayKey="draft:1" dark />);
    expect(effect.ripple).toHaveBeenCalledTimes(2);
    expect(createWelcomeDotsEffect).toHaveBeenCalledTimes(1);
    unmount();
    expect(effect.destroy).toHaveBeenCalledTimes(1);
});
