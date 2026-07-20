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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import type { AGENT_TOOL_REQUEST, AGENT_TOOL_RESPONSE, AGENT_TOOL_ERROR } from './constants';

/** iframe → parent 的工具调用请求 */
export interface ToolRequestMessage {
    event: typeof AGENT_TOOL_REQUEST;
    /** 工具名，如 `observe`，由 parent 侧 FrontendAgentToolBridgeServer 按 tool 分发到对应 handler */
    tool: string;
    requestId: string;
    body?: unknown;
}

/** parent → iframe 的工具调用成功响应 */
export interface ToolResponseMessage<T = unknown> {
    event: typeof AGENT_TOOL_RESPONSE;
    requestId: string;
    body: T;
}

/** parent → iframe 的工具调用失败响应 */
export interface ToolErrorMessage {
    event: typeof AGENT_TOOL_ERROR;
    requestId: string;
    error: {
        code?: string;
        message?: string;
    };
}

/** iframe 侧 FrontendAgentToolBridgeClient 的构造参数 */
export interface FrontendAgentToolBridgeClientOptions {
    /** 请求超时时间（毫秒），默认 3000 */
    timeoutMs?: number;
}

/** framework 侧 FrontendAgentToolBridgeServer 的构造参数 */
export interface FrontendAgentToolBridgeServerOptions {
    /** 返回 AcpSession iframe 元素（延迟解析，便于在 ref 就绪前构造） */
    targetFrame: () => HTMLIFrameElement | null;
}

/** 工具调用 handler：由调用方注册，返回响应 body 或抛错 */
export type ToolHandler = (requestBody: unknown) => Promise<unknown> | unknown;
