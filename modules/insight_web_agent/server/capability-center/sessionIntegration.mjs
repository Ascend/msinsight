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
import { CAPABILITY_MCP_NAME, CAPABILITY_MCP_PATH } from "./definitions.mjs";

/**
 * 把能力中心作为 HTTP MCP Server 注入 ACP 的 session/new、session/load、session/resume。
 * 本模块只决定“本次 ACP 请求携带哪些 mcpServers”，不参与 tools/list、tools/call 或业务执行。
 *
 * OpenCode 当前把 ACP 注入的 MCP Client 保存为目录级全局连接，同名重复注册会关闭旧连接，
 * 因此同一 OpenCode 进程只在首次成功建立 MCP 连接时注入，后续 Session 复用该连接。
 */
export const createCapabilitySessionIntegration = ({ baseUrl, accessToken, state, connectionVersion = () => 0, hasConnections = () => false, enabledForAgent } = {}) => {
    // 这三个状态只服务 OpenCode 重复注册兼容，不代表 Capability Center 是 Session-scoped。
    let openCodeInjected = false;
    let injectionQueue = Promise.resolve();
    let generation = 0;

    // Native Agent 已通过直接 Adapter 接入，外部 Agent 只有声明 HTTP MCP capability 才能注入。
    const isEnabled = () => Boolean(state.agentCapabilities?.mcp?.http)
        && (enabledForAgent?.(state.activeAgentName) ?? state.activeAgentName !== "msinsight-native");
    const isOpenCode = () => String(state.agentInfo?.name ?? "").toLowerCase() === "opencode";
    const mcpServers = () => [{
        type: "http",
        name: CAPABILITY_MCP_NAME,
        url: new URL(CAPABILITY_MCP_PATH, baseUrl).toString(),
        headers: [{ name: "Authorization", value: `Bearer ${accessToken}` }],
    }];

    const withMcpServers = async (operation) => {
        if (!isEnabled()) return operation([]);
        if (!isOpenCode()) return operation(mcpServers());
        // 标记与真实连接双重确认：连接丢失后，下一个 ACP Session 必须重新注入。
        if (openCodeInjected && hasConnections()) return operation([]);
        openCodeInjected = false;

        // OpenCode 按目录和名称保存 ACP 注入的 MCP Client，重复注册同名 Server 会替换并关闭旧连接。
        const previous = injectionQueue;
        let release;
        injectionQueue = new Promise((resolve) => { release = resolve; });
        await previous;
        if (openCodeInjected) {
            release();
            return operation([]);
        }
        // 仅当 MCP initialize 确实完成才记为已注入；ACP session 创建成功并不代表 MCP 连接成功。
        const currentGeneration = generation;
        const previousConnectionVersion = connectionVersion();
        try {
            const result = await operation(mcpServers());
            if (currentGeneration === generation && connectionVersion() > previousConnectionVersion) {
                openCodeInjected = true;
            }
            return result;
        } finally {
            release();
        }
    };

    // Agent 进程切换或 transport 异常后，下一进程必须重新接收 MCP 配置。
    const reset = () => {
        generation += 1;
        openCodeInjected = false;
    };

    return { reset, withMcpServers };
};
