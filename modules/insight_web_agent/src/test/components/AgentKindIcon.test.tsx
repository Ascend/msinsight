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
import { render } from '@testing-library/react';
import '@testing-library/jest-dom';
import { themeInstance } from '@insight/lib/theme';
import { AgentKindIcon } from '../../components/AgentKindIcon';

const renderIcon = (name: string, mode: 'light' | 'dark') => {
    const { container } = render(
        <ThemeProvider theme={themeInstance.getTheme()[mode]}>
            <AgentKindIcon name={name} />
        </ThemeProvider>,
    );
    return container.querySelector('[data-agent-icon]') as HTMLImageElement | null;
};

test('switches OpenCode and Codex between dedicated light and dark SVGs', () => {
    const lightOpenCode = renderIcon('OpenCode(auto)', 'light');
    const darkOpenCode = renderIcon('OpenCode(auto)', 'dark');
    expect(lightOpenCode).toHaveAttribute('data-agent-icon', 'themed');
    expect(darkOpenCode).toHaveAttribute('data-agent-icon', 'themed');
    expect(lightOpenCode?.getAttribute('src')).toBe('logo-opencode.svg');
    expect(darkOpenCode?.getAttribute('src')).toBe('logo-opencode-dark.svg');

    const lightCodex = renderIcon('Codex(auto)', 'light');
    const darkCodex = renderIcon('Codex(auto)', 'dark');
    expect(lightCodex?.getAttribute('src')).toBe('logo-openai.svg');
    expect(darkCodex?.getAttribute('src')).toBe('logo-openai-dark.svg');
});

test('keeps Claude, Trae, and the built-in agent on a fixed colorful logo', () => {
    const lightClaude = renderIcon('Claude Code(auto)', 'light');
    const darkClaude = renderIcon('Claude Code(auto)', 'dark');
    expect(lightClaude).toHaveAttribute('data-agent-icon', 'static');
    expect(lightClaude?.getAttribute('src')).toBe(darkClaude?.getAttribute('src'));
    expect(lightClaude?.getAttribute('src')).toBe('logo-claude.svg');

    const lightTrae = renderIcon('Trae(auto)', 'light');
    const darkTrae = renderIcon('Trae(auto)', 'dark');
    expect(lightTrae).toHaveAttribute('data-agent-icon', 'static');
    expect(lightTrae?.getAttribute('src')).toBe(darkTrae?.getAttribute('src'));
    expect(lightTrae?.getAttribute('src')).toBe('logo-trae.svg');

    const insight = renderIcon('msinsight-native', 'dark');
    expect(insight).toHaveAttribute('data-agent-icon', 'static');
    expect(insight?.tagName).toBe('IMG');
});

test('does not render a brand icon for copied or custom agent names', () => {
    expect(renderIcon('OpenCode', 'light')).toBeNull();
    expect(renderIcon('Claude Code', 'dark')).toBeNull();
});
