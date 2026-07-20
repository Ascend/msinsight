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
import type { FrontendAgentToolBridgeServer, ToolHandler } from '@insight/lib/FrontendAgentToolBridge';

/**
 * 前端 Agent 工具调用注册表（模块级单例）。
 *
 * 背景：
 * `FrontendAgentToolBridgeServer` 由 `WebAgentSessionPanel` 实例化（它持有 AcpSession iframe
 * 的 ref，用于接收请求和回复）。但工具的"实现"可能分散在不同模块：
 *   - `observe` 工具读 framework 自身状态（activeModule/session/...）
 *   - 未来 `timelineQuery` 等工具需通过 `connector.send({ to: 'Timeline', ... })` 转发
 *
 * 因此把"工具注册"和"server 实例化"解耦：`WebAgentSessionPanel` 只负责创建 server
 * 并调用 `setFrontendAgentToolBridgeServer`；具体工具实现放在 `tools/<toolName>/` 下，通过
 * `registerFrontendAgentTool` 注册自己的 handler。
 *
 * 生命周期：
 *   1. WebAgentSessionPanel mount → `new FrontendAgentToolBridgeServer(...)` → `setFrontendAgentToolBridgeServer(s)`
 *   2. 工具模块（如 `tools/observe`）注册 handler → `registerFrontendAgentTool('observe', handler)`
 *   3. WebAgentSessionPanel unmount → `setFrontendAgentToolBridgeServer(null)` → 所有 handler 自动清空
 *
 * 注意：在 server 未设置时注册的工具会被缓存，待 server 就绪后批量注册。
 */
let currentServer: FrontendAgentToolBridgeServer | null = null;
const pendingHandlers = new Map<string, ToolHandler>();

/**
 * 由 `WebAgentSessionPanel` 在 mount 时调用，设置当前可用的 frontend agent tool bridge server。
 * 传入 null 时清空所有已注册 handler。
 */
export function setFrontendAgentToolBridgeServer(server: FrontendAgentToolBridgeServer | null): void {
    currentServer = server;
    if (server !== null) {
        // 把 pending 的 handler 批量注册到新 server
        pendingHandlers.forEach((handler, tool) => server.handle(tool, handler));
    }
}

/**
 * 注册前端 agent 工具 handler。任何模块都可调用。
 *
 * @param tool 工具名（如 `'observe'`，建议使用 `@insight/lib/FrontendAgentToolBridge` 的常量）
 * @param handler 处理函数，接收请求 body，返回响应 body 或抛错
 */
export function registerFrontendAgentTool(tool: string, handler: ToolHandler): void {
    pendingHandlers.set(tool, handler);
    currentServer?.handle(tool, handler);
}

/**
 * 注销前端 agent 工具 handler。
 */
export function unregisterFrontendAgentTool(tool: string): void {
    pendingHandlers.delete(tool);
    currentServer?.unhandle(tool);
}
