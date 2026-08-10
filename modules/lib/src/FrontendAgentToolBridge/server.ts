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
import { AGENT_TOOL_REQUEST, AGENT_TOOL_RESPONSE, AGENT_TOOL_ERROR } from './constants';
import type { ToolRequestMessage, FrontendAgentToolBridgeServerOptions, ToolHandler } from './types';

/**
 * FrontendAgentToolBridgeServer：framework 侧 agent 工具调用桥接。
 *
 * 由 framework 的 `WebAgentSessionPanel` 实例化，用于接收 AcpSession iframe 发来的
 * 工具调用请求并返回响应。支持注册多个工具 handler，按请求消息的 `tool` 字段分发。
 *
 * **不自注册 `message` listener**：调用方（`WebAgentSessionPanel`）维护单一 listener，
 * 按 `event` 分发；工具调用请求通过 `handleMessage(event)` 委托给本类处理。这样避免
 * framework 侧出现多个并行 listener（reviewer 意见 #1 的核心诉求）。
 *
 * 不走 `ServerConnector.awaitFetch`，因为 `awaitFetch` 配套 `connector.fetch` 的
 * `event='request'`+`id` 协议并路由到 `requestModule`（模块数据请求），不能用于
 * 自定义工具调用契约。详见 `./constants.ts` 的设计说明。
 */
export class FrontendAgentToolBridgeServer {
    private readonly options: FrontendAgentToolBridgeServerOptions;
    private readonly handlers = new Map<string, ToolHandler>();

    constructor(options: FrontendAgentToolBridgeServerOptions) {
        this.options = options;
    }

    /**
     * 注册工具调用 handler。当收到 `AGENT_TOOL_REQUEST` 且 `tool` 匹配时调用对应 handler，
     * 将其返回值作为响应 body 回复；handler 抛错时回复 `AGENT_TOOL_ERROR`。
     *
     * 同一工具名重复注册会覆盖旧 handler。
     *
     * @param tool 工具名（如 `observe`）
     * @param handler 处理函数，接收请求 body，返回响应 body 或抛错
     */
    handle(tool: string, handler: ToolHandler): void {
        this.handlers.set(tool, handler);
    }

    /**
     * 移除已注册的工具 handler。
     */
    unhandle(tool: string): void {
        this.handlers.delete(tool);
    }

    /**
     * 处理由调用方分发的 `message` 事件。
     *
     * 调用方应在自己的 `message` listener 中按 `event` 分发：当 `event` 为
     * `AGENT_TOOL_REQUEST` 时调用本方法。非工具调用事件会被忽略。
     *
     * 注意：本方法是同步入口，但 handler 可能是 async；async handler 的响应在
     * Promise resolve 后异步发送。
     */
    handleMessage(event: MessageEvent<ToolRequestMessage>): void {
        if (event.data?.event !== AGENT_TOOL_REQUEST || !event.data.requestId) return;
        if (!event.data.tool) return;

        const source = event.source as Window | null;
        if (!source || !this.isAcpSessionSource(source)) return;

        const handler = this.handlers.get(event.data.tool);
        if (!handler) {
            source.postMessage(
                {
                    event: AGENT_TOOL_ERROR,
                    requestId: event.data.requestId,
                    error: {
                        code: 'TOOL_NOT_FOUND',
                        message: `Tool '${event.data.tool}' is not registered`,
                    },
                },
                this.responseTargetOrigin(event.origin),
            );
            return;
        }

        const requestId = event.data.requestId;
        const requestBody = event.data.body;
        const targetOrigin = this.responseTargetOrigin(event.origin);

        // handler 可能是 sync 或 async；统一用 Promise.resolve 包装
        Promise.resolve()
            .then(() => handler(requestBody))
            .then(
                (body) => {
                    source.postMessage(
                        { event: AGENT_TOOL_RESPONSE, requestId, body },
                        targetOrigin,
                    );
                },
                (error: unknown) => {
                    source.postMessage(
                        {
                            event: AGENT_TOOL_ERROR,
                            requestId,
                            error: {
                                code: 'TOOL_FAILED',
                                message: error instanceof Error ? error.message : String(error),
                            },
                        },
                        targetOrigin,
                    );
                },
            );
    }

    dispose(): void {
        this.handlers.clear();
    }

    private isAcpSessionSource(source: Window): boolean {
        const frame = this.options.targetFrame();
        return frame?.contentWindow === source;
    }

    private responseTargetOrigin(origin: string): string {
        return origin === 'null' ? '*' : origin;
    }
}
