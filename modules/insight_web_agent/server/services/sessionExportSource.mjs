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
import { createAcpAdapter } from "../infrastructure/acpAdapter.mjs";
import { normalizeAgentCapabilities } from "./capabilityService.mjs";
import { filterHistoryMessages, isHiddenResource } from "./historyContentFilter.mjs";
import { ensureHistoryMessage } from "./historyMessageService.mjs";

const error = (code, message) => Object.assign(new Error(message), { code });
const hasCapability = (value) => value !== undefined && value !== null && value !== false;
const normalizeKind = (kind) => String(kind ?? "").replace(/[A-Z]/g, (letter) => `_${letter.toLowerCase()}`).replace(/^_/, "");
const toText = (value) => value === undefined || value === null ? undefined : typeof value === "string" ? value : JSON.stringify(value);
const status = (value) => value === undefined ? undefined : ["completed", "success", "succeeded", "done"].includes(String(value).toLowerCase()) ? "completed" : ["failed", "error", "cancelled", "canceled"].includes(String(value).toLowerCase()) ? "failed" : "in_progress";

/** One detached ACP connection per owner, without chat state, MCP, file or permission handlers. */
export const createSessionExportSource = async ({
    agentServer, cwd, requestTimeoutMs = 30_000, signal,
    maxHistoryBytes = 8 * 1024 * 1024,
    hiddenPromptTexts = [],
    createAdapter = createAcpAdapter,
}) => {
    // Keep stderr drained: the adapter logs byte counts only, never agent output.
    const adapter = createAdapter({ agentServer, cwd, requestTimeoutMs, forwardStderr: true });
    let closed = false;
    let closing;
    let collector;
    const unsubscribe = adapter.onMessage((notification) => {
        if (closed || !collector || notification.method !== "session/update") return;
        const params = notification.params ?? {};
        if ((params.sessionId ?? params.session_id) !== collector.sessionId) return;
        // Bound the full incoming replay (not just recognized/searchable text).
        collector.bytes += Buffer.byteLength(JSON.stringify(notification), "utf8");
        if (collector.bytes > maxHistoryBytes) { collector.overflow = true; return; }
        collectUpdate(collector, params.update ?? params.sessionUpdate ?? params.session_update ?? params);
    });
    const close = () => {
        if (closing) return closing;
        closed = true;
        collector = undefined;
        unsubscribe?.();
        signal?.removeEventListener("abort", onAbort);
        closing = Promise.resolve().then(() => adapter.disconnect());
        return closing;
    };
    const onAbort = () => { close().catch(() => {}); };
    signal?.addEventListener("abort", onAbort, { once: true });
    const assertOpen = () => { if (closed) throw error("session_export_source_closed", "Export source is closed"); };
    try {
        if (signal?.aborted) await close();
        assertOpen();
        const initialized = await adapter.request("initialize", {
            protocolVersion: 1,
            clientCapabilities: { fs: { readTextFile: false, writeTextFile: false }, terminal: false, auth: { terminal: false } },
            clientInfo: { name: "insight-web-agent", version: "0.1.0" },
        });
        assertOpen();
        const capabilities = normalizeAgentCapabilities(initialized?.agentCapabilities ?? initialized?.agent_capabilities ?? {});
        const info = initialized?.agentInfo ?? initialized?.agent_info ?? {};
        const agentInfo = { name: typeof info.name === "string" ? info.name : null, version: typeof info.version === "string" ? info.version : null };
        const readonlyExport = initialized?._meta?.["msinsight.dev/sessionExport"] === true;
        const native = agentServer.name === "msinsight-native" || info.name === "msinsight-native";
        if (!hasCapability(capabilities.session.list)) throw error("session_export_list_unsupported", "Agent does not support session/list");
        if (native && !readonlyExport) throw error("session_export_readonly_unsupported", "Native Agent does not advertise read-only export");
        if (!readonlyExport && !capabilities.loadSession) throw error("session_export_load_unsupported", "Agent does not support exportable session/load history; resume-only is unsupported");
        return {
            agentInfo,
            async list({ cursor } = {}) {
                assertOpen();
                return adapter.request("session/list", { cwd, ...(cursor === undefined ? {} : { cursor }) });
            },
            async load(sessionId) {
                assertOpen();
                if (collector) throw error("session_export_busy", "Another history load is running");
                const current = { sessionId, messages: [], bytes: 0, overflow: false };
                collector = current;
                try {
                    if (readonlyExport) {
                        const result = await adapter.request("_msinsight/session/export", { sessionId });
                        if (!Array.isArray(result?.messages)) throw error("session_export_history_invalid", "Agent returned malformed read-only history");
                        if (Buffer.byteLength(JSON.stringify(result.messages), "utf8") > maxHistoryBytes) throw error("session_export_history_limit", "Session history size limit reached");
                        return filterHistoryMessages(structuredClone(result.messages), hiddenPromptTexts);
                    }
                    // session/resume is not a substitute: it need not replay any history.
                    await adapter.request("session/load", { sessionId, cwd, additionalDirectories: [], mcpServers: [] });
                    if (current.overflow) throw error("session_export_history_limit", "Session history size limit reached");
                    return filterHistoryMessages(current.messages, hiddenPromptTexts);
                } catch (failure) {
                    if (/session.*(?:busy|running prompt)|running prompt/i.test(failure?.message ?? "")) throw error("session_export_busy", "Session has a running prompt");
                    throw failure;
                } finally { if (collector === current) collector = undefined; }
            },
            close,
        };
    } catch (failure) {
        await close().catch(() => {});
        throw failure;
    }
};

const collectUpdate = (collector, update) => {
    const kind = normalizeKind(update.sessionUpdate ?? update.session_update ?? update.type ?? update.kind);
    if (kind === "tool_call" || kind === "tool_call_update") {
        const source = update.toolCall ?? update.tool_call ?? update;
        const toolCallId = String(source.toolCallId ?? source.tool_call_id ?? source.id ?? "").trim();
        if (!toolCallId) return;
        const message = ensureHistoryMessage(collector.messages, "assistant", update.messageId ?? update.message_id);
        const block = message.content.find((part) => part.type === "tool" && part.toolCall.toolCallId === toolCallId);
        const fields = {
            ...source,
            toolCallId,
            name: source.name ?? source.title ?? source.toolName ?? source.tool_name,
            status: status(source.status),
            input: toText(source.input ?? source.rawInput ?? source.raw_input),
            output: toText(source.output ?? source.rawOutput ?? source.raw_output),
            startedAt: source.startedAt ?? source.started_at,
            durationMs: source.durationMs ?? source.duration_ms,
        };
        const toolCall = { name: "Tool", status: "in_progress", ...(block?.toolCall ?? {}), ...Object.fromEntries(Object.entries(fields).filter(([, value]) => value !== undefined)) };
        if (block) block.toolCall = toolCall;
        else message.content.push({ id: toolCallId, type: "tool", toolCall });
        return;
    }
    if (!["user_message_chunk", "agent_message_chunk", "agent_thought_chunk"].includes(kind)) return;
    const role = kind === "user_message_chunk" ? "user" : "assistant";
    const content = Array.isArray(update.content) ? update.content : update.content === undefined ? [{ type: "text", text: update.text }] : [update.content];
    for (const value of content) {
        const block = typeof value === "string" ? { type: "text", text: value } : value;
        if (!block || typeof block !== "object") continue;
        // Hidden Host prompts are not conversation history.
        if (isHiddenResource(block)) {
            continue;
        }
        const message = ensureHistoryMessage(collector.messages, role, update.messageId ?? update.message_id);
        const type = kind === "agent_thought_chunk" ? "thinking" : block.type;
        const previous = message.content.at(-1);
        if (["text", "thinking"].includes(type) && typeof block.text === "string" && previous?.type === type && (!block.id || block.id === previous.id)) {
            previous.text += block.text;
        } else {
            // Keep supported ACP image/resource/audio fields intact rather than truncating
            // tool output or collapsing non-text blocks into keyword-searchable text.
            message.content.push({ ...structuredClone(block), id: block.id ?? randomUUID(), type });
        }
    }
};
