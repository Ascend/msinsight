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

export type PermissionDecision = 'allow_once' | 'allow_always' | 'deny';
export type PermissionState = 'pending' | 'allowed_once' | 'allowed_always' | 'denied' | 'expired' | 'invalidated';

export interface PermissionRequestItem {
    sessionId: string;
    requestId: string;
    kind?: 'filesystem' | 'bash' | 'tool';
    title?: string;
    target: string;
    path?: string;
    details?: Record<string, unknown>;
    actions: PermissionDecision[];
    state: PermissionState;
    error?: string;
    loadingDecision?: PermissionDecision;
}

export type ToolCallStatus = 'in_progress' | 'completed' | 'failed';
export type AgentActivity = 'analyzing_tool_results' | {
    type: 'model_retry';
    attempt?: number;
    maxAttempts?: number;
    retryAfterSeconds?: number;
};

export interface ToolCallItem {
    toolCallId: string;
    name: string;
    status: ToolCallStatus;
    input?: string;
    progress?: string;
    output?: string;
    startedAt?: number;
    durationMs?: number;
}

export interface ActionItem {
    actionId: string;
    label: string;
    description: string;
    command: string;
    args: Record<string, unknown>;
}

export type MessageContentBlock =
    | { id: string; type: 'text'; text: string }
    | { id: string; type: 'thinking'; text: string }
    | { id: string; type: 'tool'; toolCall: ToolCallItem };

export interface ChatMessage {
    id: string;
    role: MessageRole;
    content: MessageContentBlock[];
    activity?: AgentActivity;
    pending?: boolean;
    startedAt?: number;
    durationMs?: number;
    permission?: PermissionRequestItem;
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
    primaryAgentId?: string;
    primaryAgentName?: string;
    isPending?: boolean;
    pendingPrompt?: boolean;
    status?: SessionStatus;
}

export interface ConfigOptionValue {
    value: string;
    name: string;
    description?: string;
    source?: {
        id?: string;
        kind?: string;
    };
    diagnostics?: Array<{
        code?: string;
        message?: string;
    }>;
    available?: boolean;
    _meta?: {
        'msinsight.dev/source'?: {
            id?: string;
            kind?: string;
        };
        'msinsight.dev/available'?: boolean;
        'msinsight.dev/diagnostics'?: Array<{
            code?: string;
            message?: string;
        }>;
    };
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

export interface AgentConfigServer {
    name: string;
    command: string;
    args: string[];
    env: Record<string, string>;
}

export interface BuiltinAgentConfig {
    schemaVersion: number;
    name: 'msinsight-native';
    provider: string;
    model: string;
    baseUrl: string;
    apiKey: string;
}

export interface AgentSessionConfig {
    requestTimeoutMs: number;
    promptRequestTimeoutMs: number;
    permissionRequestTimeoutMs: number;
    defaultAllowlist: {
        includeDocsRoot: boolean;
        includeAgentWorkspaceRoot: boolean;
        includeProjectRoot: boolean;
        extraPaths: string[];
    };
}

export interface AgentConfigSnapshot {
    activeAgentName: string;
    agentServers: AgentConfigServer[];
    builtinAgent: BuiltinAgentConfig;
    sessionConfig: AgentSessionConfig;
}

export interface AgentConfigSaveResult {
    ok?: boolean;
    snapshot?: AgentConfigSnapshot;
    error?: string;
    message?: string;
    saved?: boolean;
    details?: Array<{ field?: string; message: string }>;
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

export interface AvailableCapability {
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
    availableCapabilities?: AvailableCapability[];
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
}

export type ServerEvent =
    | { type: 'state'; state: Partial<AppState> }
    | { type: 'agent_discovery_started' }
    | { type: 'agent_discovery_completed'; runtimeChanged?: boolean }
    | { type: 'message_added'; sessionId?: string; message: ChatMessage }
    | { type: 'message_content_delta'; sessionId?: string; id: string; blockId: string; blockType: 'text' | 'thinking'; delta: string }
    | { type: 'message_content_added'; sessionId?: string; id: string; block: MessageContentBlock }
    | { type: 'message_tool_call'; sessionId?: string; id: string; toolCall: ToolCallItem }
    | { type: 'message_activity'; sessionId?: string; id: string; activity?: AgentActivity }
    | { type: 'message_removed'; sessionId?: string; id: string }
    | { type: 'config_options'; sessionId?: string; configOptions: ConfigOption[] }
    | { type: 'permission_request'; sessionId: string; requestId: string; kind?: 'filesystem' | 'bash' | 'tool'; title?: string; target?: string; path?: string; details?: Record<string, unknown>; actions: PermissionDecision[] }
    | { type: 'permission_resolved'; sessionId: string; requestId: string; state: Exclude<PermissionState, 'pending'> }
    | { type: 'prompt_status'; sessionId?: string; pendingPrompt: boolean }
    | { type: 'frontend_command_request'; requestId: string; sessionId?: string; command: string; args: Record<string, unknown>; deadline: number }
    | { type: 'frontend_command_cancel'; requestId: string; reason?: string };
