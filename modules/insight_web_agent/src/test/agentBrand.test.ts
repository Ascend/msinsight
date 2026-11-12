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
import { agentKindLogoAsset, agentKindLogoSrc, resolveAgentKind } from '../agentBrand';

test('resolves logos for the built-in agent and the four auto-discovered catalog agents', () => {
    expect(resolveAgentKind({ name: 'msinsight-native' })).toBe('insight');
    expect(resolveAgentKind({ name: 'OpenCode' })).toBe('opencode');
    expect(resolveAgentKind({ name: 'Claude Code' })).toBe('claude');
    expect(resolveAgentKind({ name: 'Codex' })).toBe('codex');
    expect(resolveAgentKind({ name: 'Trae' })).toBe('trae');
    expect(resolveAgentKind({ name: ' claude code ' })).toBe('claude');
});

test('uses light and dark SVGs for OpenCode and Codex, and a fixed colorful logo for Claude, Trae, and Insight', () => {
    expect(agentKindLogoAsset({ name: 'msinsight-native' })?.darkSrc).toBeUndefined();
    expect(agentKindLogoAsset({ name: 'Claude Code' })?.darkSrc).toBeUndefined();
    expect(agentKindLogoAsset({ name: 'Trae' })?.darkSrc).toBeUndefined();
    expect(agentKindLogoAsset({ name: 'OpenCode' })?.darkSrc).toBeDefined();
    expect(agentKindLogoAsset({ name: 'Codex' })?.darkSrc).toBeDefined();
    expect(agentKindLogoSrc({ name: 'OpenCode' }, 'light')).not.toBe(agentKindLogoSrc({ name: 'OpenCode' }, 'dark'));
    expect(agentKindLogoSrc({ name: 'Codex' }, 'light')).not.toBe(agentKindLogoSrc({ name: 'Codex' }, 'dark'));
    expect(agentKindLogoSrc({ name: 'Claude Code' }, 'light')).toBe(agentKindLogoSrc({ name: 'Claude Code' }, 'dark'));
    expect(agentKindLogoSrc({ name: 'Trae' }, 'light')).toBe(agentKindLogoSrc({ name: 'Trae' }, 'dark'));
});

test('does not match copies or similar names', () => {
    expect(resolveAgentKind({ name: 'OpenCode Copy' })).toBeUndefined();
    expect(resolveAgentKind({ name: 'Claude Code Copy' })).toBeUndefined();
    expect(resolveAgentKind({ name: 'MS Insight_Native' })).toBeUndefined();
    expect(resolveAgentKind({ name: 'DeepSeek' })).toBeUndefined();
    expect(resolveAgentKind({ name: '盘古大模型' })).toBeUndefined();
    expect(resolveAgentKind({ name: 'Custom Agent' })).toBeUndefined();
    expect(resolveAgentKind({})).toBeUndefined();
});
