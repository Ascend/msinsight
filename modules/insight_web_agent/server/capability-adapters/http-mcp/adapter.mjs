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
import { randomUUID } from "node:crypto";
import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StreamableHTTPServerTransport } from "@modelcontextprotocol/sdk/server/streamableHttp.js";
import { CallToolRequestSchema, ListToolsRequestSchema } from "@modelcontextprotocol/sdk/types.js";
import { CAPABILITY_MCP_SERVER_INFO } from "../../capability-center/definitions.mjs";
import { json } from "../../http/response.mjs";
import { tokensEqual } from "../../http/security.mjs";

const MAX_MCP_SESSIONS = 64;

/**
 * 将协议无关 Capability Center 投影为 MCP Streamable HTTP Server。
 * sessions 只保存 MCP 协议状态，用于跨 HTTP 请求关联响应与取消，不保存 Tool 业务状态。
 */
export const createHttpMcpAdapter = ({ capabilityCenter, accessToken } = {}) => {
    const sessions = new Map();
    let connectionVersion = 0;

    const handle = async (req, res) => {
        if (!tokensEqual(accessToken, bearerToken(req.headers.authorization))) {
            return json(res, { error: "Unauthorized" }, 401);
        }

        // initialize 请求没有 session header；后续请求必须携带 SDK 生成的 MCP Session ID。
        const sessionId = String(req.headers["mcp-session-id"] ?? "");
        let entry = sessionId ? sessions.get(sessionId) : undefined;
        if (!entry) {
            if (req.method !== "POST" || sessionId) {
                return json(res, {
                    jsonrpc: "2.0",
                    error: { code: -32000, message: sessionId ? "MCP session not found" : "MCP session is required" },
                    id: null,
                }, sessionId ? 404 : 400);
            }
            if (sessions.size >= MAX_MCP_SESSIONS) {
                return json(res, {
                    jsonrpc: "2.0",
                    error: { code: -32000, message: "Too many MCP sessions" },
                    id: null,
                }, 503);
            }
            entry = await createMcpSession({
                capabilityCenter,
                sessions,
                onInitialize: () => { connectionVersion += 1; },
            });
        }

        // HTTP 请求异常断开时关闭 MCP Server，使 SDK 的 AbortSignal 继续传播到前端 Command。
        const closeOnDisconnect = () => {
            if (res.writableEnded) return;
            const activeSessionId = entry.transport.sessionId;
            if (activeSessionId) sessions.delete(activeSessionId);
            void entry.server.close().catch((error) => {
                console.warn(`Failed to close disconnected MCP session: ${error.message}`);
            });
        };
        res.once("close", closeOnDisconnect);
        try {
            await entry.transport.handleRequest(req, res);
        } catch (error) {
            if (!res.headersSent) {
                json(res, {
                    jsonrpc: "2.0",
                    error: { code: -32603, message: "Internal server error" },
                    id: null,
                }, 500);
            }
            console.error(`HTTP MCP request failed: ${error.message}`);
        } finally {
            res.off("close", closeOnDisconnect);
            if (!entry.transport.sessionId || req.method === "DELETE") await entry.server.close();
        }
    };

    const close = async () => {
        const entries = [...sessions.values()];
        sessions.clear();
        await Promise.allSettled(entries.map(({ server }) => server.close()));
    };

    return {
        close,
        // Session Integration 用这两个只读信号判断 OpenCode 是否真的完成过 MCP initialize、当前连接是否仍存活。
        connectionVersion: () => connectionVersion,
        hasConnections: () => sessions.size > 0,
        handle,
    };
};

const createMcpSession = async ({ capabilityCenter, sessions, onInitialize }) => {
    const server = new Server(
        CAPABILITY_MCP_SERVER_INFO,
        { capabilities: { tools: {} }, instructions: "Use the available MindStudio Insight capabilities when they are relevant to the user's task." },
    );
    const transport = new StreamableHTTPServerTransport({
        sessionIdGenerator: randomUUID,
        enableJsonResponse: true,
        onsessioninitialized(sessionId) {
            sessions.set(sessionId, { server, transport });
        },
        onsessionclosed(sessionId) {
            sessions.delete(sessionId);
        },
    });
    server.oninitialized = onInitialize;
    // tools/list 始终从 Registry 实时投影，不在 Adapter 中复制或缓存 Tool 定义。
    server.setRequestHandler(ListToolsRequestSchema, async () => ({ tools: capabilityCenter.list() }));
    server.setRequestHandler(CallToolRequestSchema, async (request, extra) => {
        try {
            const result = await capabilityCenter.invoke({
                invocationId: `mcp:${String(extra.requestId)}:${randomUUID()}`,
                name: request.params.name,
                input: request.params.arguments ?? {},
                signal: extra.signal,
            });
            return capabilityResult(result);
        } catch (error) {
            return capabilityFailure(error);
        }
    });
    await server.connect(transport);
    return { server, transport };
};

const capabilityResult = (result) => ({
    content: [{ type: "text", text: serialize(result) }],
    ...(isRecord(result) ? { structuredContent: result } : {}),
});

const capabilityFailure = (error) => {
    const body = {
        ok: false,
        error: {
            code: error.code ?? "CAPABILITY_EXECUTION_FAILED",
            message: error.message,
            retryable: Boolean(error.retryable),
            ...(error.details === undefined ? {} : { details: error.details }),
        },
    };
    return {
        content: [{ type: "text", text: serialize(body) }],
        structuredContent: body,
        isError: true,
    };
};

const bearerToken = (authorization) => {
    const match = String(authorization ?? "").match(/^Bearer\s+(.+)$/i);
    return match?.[1] ?? "";
};
const isRecord = (value) => Boolean(value) && typeof value === "object" && !Array.isArray(value);
const serialize = (value) => typeof value === "string" ? value : JSON.stringify(value ?? null);
