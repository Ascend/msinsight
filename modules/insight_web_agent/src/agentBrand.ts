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
import logoClaude from './icons/logo-claude.svg';
import logoDeepseek from './icons/logo-deepseek.svg';
import logoInsight from './icons/logo-insight.svg';
import logoOpenai from './icons/logo-openai.svg';
import logoPangu from './icons/logo-pangu.svg';

export type AgentKind = 'insight' | 'claude' | 'deepseek' | 'pangu' | 'openai';

export const AGENT_KIND_LOGOS: Record<AgentKind, string> = {
    insight: logoInsight,
    claude: logoClaude,
    deepseek: logoDeepseek,
    pangu: logoPangu,
    openai: logoOpenai,
};

const KIND_PATTERNS: Array<{ kind: AgentKind; pattern: RegExp }> = [
    { kind: 'insight', pattern: /msinsight|mindstudio|insight/i },
    { kind: 'pangu', pattern: /pangu|盘古/i },
    { kind: 'deepseek', pattern: /deepseek/i },
    { kind: 'claude', pattern: /claude|anthropic/i },
    { kind: 'openai', pattern: /openai|chatgpt|\bcodex\b/i },
];

export const resolveAgentKind = ({
    name = '',
    command = '',
    provider = '',
}: {
    name?: string;
    command?: string;
    provider?: string;
} = {}): AgentKind | undefined => {
    const commandBase = command.trim().split(/[/\\]/).pop() ?? '';
    const sources = [provider, commandBase, name];
    return KIND_PATTERNS.find(({ pattern }) => sources.some((source) => pattern.test(source)))?.kind;
};

export const agentKindLogo = (input?: Parameters<typeof resolveAgentKind>[0]): string | undefined => {
    const kind = resolveAgentKind(input);
    return kind ? AGENT_KIND_LOGOS[kind] : undefined;
};
