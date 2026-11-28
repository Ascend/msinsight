/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { isToolDiagnosticId } from "../utils/toolError.mjs";

// Some agents share an HTTP MCP connection between ACP conversations. The
// terminal ACP tool update supplies the owner; the MCP response supplies the
// one-use receipt. Neither the selected chat nor the MCP connection is an owner.
const RECEIPT_TTL_MS = 60_000;
const MAX_RECEIPTS = 128;
const RECEIPT_LABEL = "Insight workspace receipt: ";
const isWorkspaceToolName = (name) => /(?:^|[.:_/-])msinsight$/i.test(name ?? "");

export const workspaceReceiptBlock = (id) => ({
    type: "text", text: RECEIPT_LABEL + id, annotations: { audience: ["assistant"] },
});

const receiptLabelId = (text) => {
    if (typeof text !== "string" || !text.startsWith(RECEIPT_LABEL)) return undefined;
    const id = text.slice(RECEIPT_LABEL.length);
    return isToolDiagnosticId(id) ? id : undefined;
};

// Strip the extra MCP content block from tool history and UI. Keep the
// business result, including structuredContent and ordinary diagnostic errors.
export const visibleWorkspaceResult = (value) => {
    if (typeof value === "string" && value.includes(RECEIPT_LABEL) && value.length <= 32768) {
        if (receiptLabelId(value.trim())) return undefined;
        try {
            const parsed = JSON.parse(value);
            const visible = visibleWorkspaceResult(parsed);
            if (visible !== parsed) return JSON.stringify(visible);
        } catch { /* Keep ordinary tool output verbatim. */ }
    }
    if (Array.isArray(value)) {
        const visible = value.filter(block => !receiptLabelId((block?.type === "content" ? block.content : block)?.text));
        return visible.length === value.length ? value : visible;
    }
    if (value && typeof value === "object") {
        if ("error" in value && value.result) {
            const result = visibleWorkspaceResult(value.result);
            if (result !== value.result) return { ...value, result };
        }
        if (Array.isArray(value.content)) {
            const content = visibleWorkspaceResult(value.content);
            if (content !== value.content) return { ...value, content };
        }
    }
    return value;
};

// ACP adapters forward MCP results as content blocks, raw JSON, or their text
// representation. Only follow these envelopes, never search arbitrary arguments,
// command results or assistant prose for a UUID.
const receiptId = (output) => {
    const queue = [output];
    for (let i = 0; i < queue.length && i < 64; i++) {
        const value = queue[i];
        if (typeof value === "string") {
            if (value.length > 32768) continue;
            const text = value.trim();
            const id = receiptLabelId(text);
            if (id) return id;
            if (/^[{\[]/.test(text)) {
                try { queue.push(JSON.parse(text)); } catch { /* Plain ACP display text. */ }
            }
        } else if (Array.isArray(value)) {
            queue.push(...value.slice(0, 16));
        } else if (value && typeof value === "object") {
            const id = value.ok === false && value.error?.details?.diagnosticId;
            if (isToolDiagnosticId(id)) return id;
            if (value.type === "text") queue.push(value.text);
            else if ("result" in value && "error" in value) queue.push(value.result); // Codex ACP rawOutput.
            else queue.push(value.content, value.structuredContent);
        }
    }
    return undefined;
};

export const createWorkspaceTurnCorrelation = ({ state, captureWorkspaceTurn, recordWorkspaceOutcome }) => {
    const pending = new Map();

    const capture = () => {
        const turns = new Map();
        for (const sessionId of state.sessionContexts.keys()) {
            const turn = captureWorkspaceTurn(sessionId);
            if (turn) turns.set(sessionId, turn);
        }
        return turns;
    };

    const remember = (id, turns, outcome, command) => {
        if (!isToolDiagnosticId(id) || !turns?.size) return;
        const now = Date.now();
        for (const [key, record] of pending) {
            if (now - record.at < RECEIPT_TTL_MS) break;
            pending.delete(key);
        }
        if (pending.size >= MAX_RECEIPTS) pending.delete(pending.keys().next().value);
        pending.set(id, { turns, outcome, command, at: now });
    };

    const consume = (sessionId, source, toolCall, previousStatus, toolCalls) => {
        const { name, toolCallId } = toolCall;
        const context = state.sessionContexts.get(sessionId);
        if (!context?.pendingPrompt || context.replayingHistory || context.refreshingConfigOnly) return;
        const turn = captureWorkspaceTurn(sessionId);
        if (!turn) return;
        const input = source.rawInput ?? source.raw_input ?? source.input;
        const toolName = source.name ?? source.toolName ?? source.tool_name;
        const previous = toolCalls?.get(toolCallId);
        const status = source.status === undefined ? previous?.status ?? toolCall.status : toolCall.status;
        // ACP titles may be display text. Explicit tool identifiers take precedence.
        const unrelated = (input?.tool !== undefined && input.tool !== "msinsight") ||
            (toolName !== undefined && !isWorkspaceToolName(toolName));
        const identified = input?.tool === "msinsight" || isWorkspaceToolName(toolName ?? name);
        const workspace = unrelated || previous?.workspace === false ? false : identified || previous?.workspace;
        const inputCommand = input?.arguments?.command ?? input?.command;
        const command = inputCommand ?? previous?.command;
        // Some clients publish terminal status and output separately. Retain identity
        // until a receipt is consumed, even if the status already arrived.
        if (toolCalls && (workspace !== undefined || typeof command === "string")) {
            if (!previous && toolCalls.size >= MAX_RECEIPTS) toolCalls.delete(toolCalls.keys().next().value);
            toolCalls.set(toolCallId, {
                workspace, status, command: workspace !== false && typeof command === "string" ? command : undefined,
            });
        }
        if (!workspace || (status !== "failed" && status !== "completed")) return;
        const id = receiptId(source.output ?? source.rawOutput ?? source.raw_output)
            ?? receiptId(source.content ?? source.message);
        const record = pending.get(id);
        if (!id && status === "completed") {
            // Some clients omit MCP content on successful calls. A matching
            // successful workspace action may clear a streak, never increase it.
            if (!identified || !inputCommand || !["pending", "in_progress"].includes(previousStatus)) return;
            const matches = [...pending].filter(([, receipt]) => receipt.command === command &&
                receipt.outcome === "completed" && receipt.turns.get(sessionId) === turn &&
                Date.now() - receipt.at < RECEIPT_TTL_MS);
            if (matches.length !== 1 || matches[0][1].turns.size !== 1) return;
            pending.delete(matches[0][0]);
            toolCalls?.delete(toolCallId);
            recordWorkspaceOutcome(sessionId, turn, "completed");
            return;
        }
        if (!record || (record.command && command && record.command !== command)) return;
        if (record.turns.get(sessionId) !== turn || Date.now() - record.at >= RECEIPT_TTL_MS) return;
        pending.delete(id);
        toolCalls?.delete(toolCallId);
        recordWorkspaceOutcome(sessionId, turn, record.outcome);
    };

    return { capture, remember, consume, has: id => pending.has(id) };
};
