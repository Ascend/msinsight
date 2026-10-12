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
import logoInsight from './icons/logo-insight.svg';
import logoOpenai from './icons/logo-openai.svg';
import logoOpenaiDark from './icons/logo-openai-dark.svg';
import logoOpencode from './icons/logo-opencode.svg';
import logoOpencodeDark from './icons/logo-opencode-dark.svg';
import logoTrae from './icons/logo-trae.svg';

export type AgentKind = 'insight' | 'opencode' | 'claude' | 'codex' | 'trae';

export type AgentLogoAsset = {
    src: string;
    darkSrc?: string;
};

export const AGENT_KIND_LOGOS: Record<AgentKind, AgentLogoAsset> = {
    insight: { src: logoInsight },
    opencode: { src: logoOpencode, darkSrc: logoOpencodeDark },
    claude: { src: logoClaude },
    codex: { src: logoOpenai, darkSrc: logoOpenaiDark },
    trae: { src: logoTrae },
};

const BRANDED_AGENT_KINDS: Array<{ kind: AgentKind; name: string }> = [
    { kind: 'insight', name: 'msinsight-native' },
    { kind: 'opencode', name: 'OpenCode(auto)' },
    { kind: 'claude', name: 'Claude Code(auto)' },
    { kind: 'codex', name: 'Codex(auto)' },
    { kind: 'trae', name: 'Trae(auto)' },
];

const normalizeAgentName = (name: string) => name.trim().toLowerCase();

export const resolveAgentKind = ({
    name = '',
}: {
    name?: string;
} = {}): AgentKind | undefined => {
    const normalizedName = normalizeAgentName(name);
    if (!normalizedName) return undefined;
    return BRANDED_AGENT_KINDS.find((entry) => normalizeAgentName(entry.name) === normalizedName)?.kind;
};

export const agentKindLogoAsset = (input?: Parameters<typeof resolveAgentKind>[0]): AgentLogoAsset | undefined => {
    const kind = resolveAgentKind(input);
    return kind ? AGENT_KIND_LOGOS[kind] : undefined;
};

export const agentKindLogoSrc = (
    input: Parameters<typeof resolveAgentKind>[0] | undefined,
    mode: 'light' | 'dark',
): string | undefined => {
    const logo = agentKindLogoAsset(input);
    if (!logo) return undefined;
    return mode === 'dark' && logo.darkSrc ? logo.darkSrc : logo.src;
};

export const agentKindLogo = (input?: Parameters<typeof resolveAgentKind>[0]): string | undefined => (
    agentKindLogoAsset(input)?.src
);
