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
import {
    AGENT_TOOL_REQUEST,
    AGENT_TOOL_RESPONSE,
    AGENT_TOOL_ERROR,
    DEFAULT_TOOL_REQUEST_TIMEOUT_MS,
} from './constants';
import type {
    ToolResponseMessage,
    ToolErrorMessage,
    FrontendAgentToolBridgeClientOptions,
} from './types';

/**
 * FrontendAgentToolBridgeClient：iframe 侧 agent 工具调用桥接。
 *
 * 由 AcpSession iframe（insight_web_agent）实例化，用于向 parent framework 发起
 * 工具调用请求并等待响应。内部封装 `requestId` 关联 + 超时 + Promise 逻辑。
 *
 * 不走 `ClientConnector.fetch`，因为 `fetch` 强制 `event='request'` 并配套
 * `ServerConnector.awaitFetch` 路由到 `requestModule`（模块数据请求），不能用于
 * 自定义工具调用契约。详见 `./constants.ts` 的设计说明。
 */
export class FrontendAgentToolBridgeClient {
    private readonly timeoutMs: number;
    private disposed = false;

    constructor(options: FrontendAgentToolBridgeClientOptions = {}) {
        this.timeoutMs = options.timeoutMs ?? DEFAULT_TOOL_REQUEST_TIMEOUT_MS;
    }

    /**
     * 发起一次工具调用，返回 Promise 等待 parent 响应。
     *
     * @param tool 工具名（如 `observe`），parent 侧按此名分发到对应 handler
     * @param requestBody 请求体（可选），透传给 parent 侧 handler
     * @throws Error 当 parent 未响应、超时、或返回错误时 reject
     */
    request<T = unknown>(tool: string, requestBody?: unknown): Promise<T> {
        if (window.parent === window) {
            return Promise.reject(new Error('Agent View is not embedded in the Insight framework'));
        }
        if (this.disposed) {
            return Promise.reject(new Error('FrontendAgentToolBridgeClient has been disposed'));
        }

        const requestId = crypto.randomUUID();
        return new Promise<T>((resolve, reject) => {
            const timeoutId = window.setTimeout(() => {
                window.removeEventListener('message', handleMessage);
                reject(new Error(`Tool '${tool}' request timed out`));
            }, this.timeoutMs);

            const finish = (callback: () => void): void => {
                window.clearTimeout(timeoutId);
                window.removeEventListener('message', handleMessage);
                callback();
            };

            const handleMessage = (event: MessageEvent<ToolResponseMessage<T> | ToolErrorMessage>): void => {
                if (event.source !== window.parent || event.data?.requestId !== requestId) return;
                if (event.data.event === AGENT_TOOL_RESPONSE) {
                    finish(() => resolve((event.data as ToolResponseMessage<T>).body as T));
                    return;
                }
                if (event.data.event === AGENT_TOOL_ERROR) {
                    const errMsg = (event.data as ToolErrorMessage).error?.message ?? `Tool '${tool}' failed`;
                    finish(() => reject(new Error(errMsg)));
                }
            };

            window.addEventListener('message', handleMessage);
            window.parent.postMessage(
                {
                    event: AGENT_TOOL_REQUEST,
                    tool,
                    requestId,
                    body: requestBody ?? {},
                },
                '*',
            );
        });
    }

    dispose(): void {
        this.disposed = true;
    }
}
