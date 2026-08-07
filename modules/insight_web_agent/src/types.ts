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
export type MessageRole = 'user' | 'assistant';

export interface ChatMessage {
    id: string;
    role: MessageRole;
    text: string;
    images?: ImageAttachment[];
    thinking?: string;
    pending?: boolean;
}

export interface ImageAttachment {
    id: string;
    name: string;
    mimeType: string;
    data: string;
}

export type SessionStatus = 'idle' | 'loading' | 'working' | 'completed' | 'error';

export interface SessionItem {
    sessionId: string;
    title?: string;
    updatedAt?: string;
    isPending?: boolean;
    pendingPrompt?: boolean;
    status?: SessionStatus;
}

export interface ConfigOptionValue {
    value: string;
    name: string;
    description?: string;
    options?: ConfigOptionValue[];
}

export interface ConfigOption {
    id: string;
    name: string;
    description?: string;
    category?: string;
    type: 'select';
    currentValue: string;
    options: ConfigOptionValue[];
}

export interface AgentServerItem {
    name: string;
}

export interface AgentInfo {
    name?: string;
    title?: string;
    version?: string;
}

export interface AvailableCommand {
    name: string;
    description?: string;
    input?: unknown;
}

export interface AvailableSkill {
    name: string;
    description?: string;
}

export interface AppState {
    initialized?: boolean;
    agentError?: string;
    agentInfo?: AgentInfo;
    agentCapabilities?: AgentCapabilities;
    availableCommands?: AvailableCommand[];
    availableSkills?: AvailableSkill[];
    sessionId?: string;
    pendingPrompt: boolean;
    messages: ChatMessage[];
    sessions: SessionItem[];
    configOptions: ConfigOption[];
    agentServers?: AgentServerItem[];
    activeAgentName?: string;
    isDraftSession?: boolean;
}

export interface AgentCapabilities {
    loadSession?: boolean;
    session?: {
        list?: boolean;
        delete?: boolean;
        resume?: boolean;
        close?: boolean;
        setConfigOption?: boolean;
    };
}

export interface SessionRecord {
    sessionId: string;
    messages: ChatMessage[];
    configOptions?: ConfigOption[];
    loaded: boolean;
    pendingPrompt: boolean;
    queuedPrompts: QueuedPrompt[];
    status: SessionStatus;
}

export interface SessionConfigUpdateResult {
    ok?: boolean;
    sessionId?: string;
    configOptions?: ConfigOption[];
    error?: string;
}

export interface QueuedPrompt {
    text: string;
    images: ImageAttachment[];
    mode?: string;
    hiddenContext?: Record<string, unknown>;
}

export type ServerEvent =
    | { type: 'state'; state: Partial<AppState> }
    | { type: 'message_added'; sessionId?: string; message: ChatMessage }
    | { type: 'message_delta'; sessionId?: string; id: string; field: 'text' | 'thinking'; delta: string }
    | { type: 'message_delta'; sessionId?: string; id: string; field: 'images'; delta: ImageAttachment[] }
    | { type: 'message_removed'; sessionId?: string; id: string }
    | { type: 'config_options'; sessionId?: string; configOptions: ConfigOption[] }
    | { type: 'prompt_status'; sessionId?: string; pendingPrompt: boolean };
