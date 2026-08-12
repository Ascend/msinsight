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
import type { AgentConfigSaveResult, AgentConfigServer, AgentConfigSnapshot, AgentServerItem, AgentSessionConfig, AppState, BuiltinAgentConfig, ChatMessage, ConfigOption, ImageAttachment, PermissionDecision, SessionConfigUpdateResult, SessionItem } from './types';
import type { HostContext } from './connection';
import { apiUrl } from './env';

interface PromptResponse {
    ok?: boolean;
    sessionId?: string;
    error?: string;
}

interface SessionsResponse {
    sessions?: SessionItem[];
    error?: string;
}

interface OkResponse {
    ok?: boolean;
    runtimeChanged?: boolean;
    sessionId?: string;
    configOptions?: ConfigOption[];
    error?: string;
}

interface AgentsResponse extends OkResponse {
    activeAgentName?: string;
    agentServers?: AgentServerItem[];
    discoveryLoading?: boolean;
}

export const fetchAgents = (): Promise<AgentsResponse> => {
    return requestJson<AgentsResponse>('/api/agents');
};

export const refreshAgents = (): Promise<OkResponse> => {
    return requestJson<OkResponse>('/api/agents/refresh', { method: 'POST' });
};

interface LoadSessionResponse extends OkResponse {
    messages?: ChatMessage[];
    pendingPrompt?: boolean;
}

const requestJson = async <T,>(path: string, init?: RequestInit): Promise<T> => {
    const response = await fetch(apiUrl(path), {
        ...init,
        headers: {
            ...(init?.body ? { 'content-type': 'application/json' } : {}),
            ...init?.headers,
        },
    });
    const body = await response.json() as T & { error?: string; message?: string; details?: unknown; saved?: boolean };
    if (!response.ok) {
        const error = new Error(body.message ?? body.error ?? response.statusText) as Error & { body?: typeof body };
        error.body = body;
        throw error;
    }
    return body;
};

export const fetchState = (): Promise<Partial<AppState>> => {
    return requestJson<Partial<AppState>>('/api/state');
};

export const fetchSessions = async (): Promise<SessionItem[]> => {
    const body = await requestJson<SessionsResponse>('/api/sessions');
    return body.sessions ?? [];
};

export const createSession = (): Promise<LoadSessionResponse> => {
    return requestJson<LoadSessionResponse>('/api/sessions/new', { method: 'POST' });
};

export const setSessionModel = (model: string, sessionId?: string): Promise<OkResponse> => {
    return requestJson<OkResponse>('/api/session-config/model', {
        method: 'POST',
        body: JSON.stringify({ model, sessionId }),
    });
};

export const setSessionMode = (mode: string, sessionId?: string): Promise<SessionConfigUpdateResult> => {
    return requestJson<SessionConfigUpdateResult>('/api/session-config/mode', {
        method: 'POST',
        body: JSON.stringify({ mode, sessionId }),
    });
};

export const loadSession = (sessionId: string): Promise<LoadSessionResponse> => {
    return requestJson<LoadSessionResponse>('/api/sessions/load', {
        method: 'POST',
        body: JSON.stringify({ sessionId }),
    });
};

export const deleteSession = (sessionId: string): Promise<OkResponse> => {
    return requestJson<OkResponse>('/api/sessions/delete', {
        method: 'POST',
        body: JSON.stringify({ sessionId }),
    });
};

export const sendPrompt = (text: string, newSession?: boolean, sessionId?: string, images: ImageAttachment[] = [], mode?: string): Promise<PromptResponse> => {
    return requestJson<PromptResponse>('/api/prompt', {
        method: 'POST',
        body: JSON.stringify({ text, newSession, sessionId, images, mode }),
    });
};

export const updateContext = (context: HostContext): Promise<OkResponse> => {
    return requestJson<OkResponse>('/api/context', {
        method: 'POST',
        body: JSON.stringify(context),
    });
};

export const updatePageObservation = (observation: Record<string, unknown>): Promise<OkResponse> => {
    return requestJson<OkResponse>('/api/page/observation', {
        method: 'POST',
        body: JSON.stringify({ observation }),
    });
};

export const fetchPageObservation = (): Promise<{ observation?: Record<string, unknown> | null; updatedAt?: number | null }> => {
    return requestJson('/api/page/observation');
};

export const cancelPrompt = (sessionId?: string): Promise<OkResponse> => {
    return requestJson<OkResponse>('/api/cancel', {
        method: 'POST',
        body: JSON.stringify({ sessionId }),
    });
};

export const switchAgent = (name: string): Promise<AgentsResponse> => {
    return requestJson<AgentsResponse>('/api/agents/switch', {
        method: 'POST',
        body: JSON.stringify({ name }),
    });
};

export const respondPermission = (sessionId: string, requestId: string, decision: PermissionDecision): Promise<OkResponse> => {
    return requestJson<OkResponse>('/api/permissions/respond', {
        method: 'POST',
        body: JSON.stringify({ sessionId, requestId, decision }),
    });
};

export const fetchAgentConfig = async (): Promise<AgentConfigSnapshot> => {
    const body = await requestJson<{ snapshot: AgentConfigSnapshot }>('/api/agent-config');
    return body.snapshot;
};

export const saveAgentServersConfig = (config: { activeAgentName: string; agentServers: AgentConfigServer[] }): Promise<AgentConfigSaveResult> => {
    return requestJson<AgentConfigSaveResult>('/api/agent-config/servers', {
        method: 'PUT',
        body: JSON.stringify(config),
    });
};

export const saveBuiltinAgentConfig = (config: BuiltinAgentConfig): Promise<AgentConfigSaveResult> => {
    return requestJson<AgentConfigSaveResult>('/api/agent-config/builtin', {
        method: 'PUT',
        body: JSON.stringify(config),
    });
};

export const saveAgentSessionConfig = (config: AgentSessionConfig): Promise<AgentConfigSaveResult> => {
    return requestJson<AgentConfigSaveResult>('/api/agent-config/session', {
        method: 'PUT',
        body: JSON.stringify(config),
    });
};
