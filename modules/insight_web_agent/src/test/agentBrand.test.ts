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
import { resolveAgentKind } from '../agentBrand';

test('resolves known agent types from name, command, or provider', () => {
    expect(resolveAgentKind({ name: 'MS Insight_Native' })).toBe('insight');
    expect(resolveAgentKind({ name: 'msinsight-native' })).toBe('insight');
    expect(resolveAgentKind({ name: 'Claude Code(auto)' })).toBe('claude');
    expect(resolveAgentKind({ command: '/usr/local/bin/claude-agent-acp' })).toBe('claude');
    expect(resolveAgentKind({ name: 'DeepSeek' })).toBe('deepseek');
    expect(resolveAgentKind({ name: '盘古大模型' })).toBe('pangu');
    expect(resolveAgentKind({ command: 'pangu-acp' })).toBe('pangu');
    expect(resolveAgentKind({ provider: 'openai' })).toBe('openai');
    expect(resolveAgentKind({ name: 'Codex(auto)', command: 'codex-acp' })).toBe('openai');
});

test('does not invent a type for unknown or OpenCode agents', () => {
    expect(resolveAgentKind({ name: 'OpenCode', command: 'opencode' })).toBeUndefined();
    expect(resolveAgentKind({ name: 'Custom Agent', command: 'my-agent' })).toBeUndefined();
    expect(resolveAgentKind({})).toBeUndefined();
});
