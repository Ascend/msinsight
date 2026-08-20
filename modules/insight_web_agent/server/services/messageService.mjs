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
export const appendChunk = ({ eventBus, state }, sessionId, role, type, delta) => {
    const message = ensureMessage({ eventBus, state }, sessionId, role);
    if (!message) return;
    const block = message.content.at(-1);
    if (!block || block.type !== type) {
        const nextBlock = { id: crypto.randomUUID(), type, text: delta };
        message.content.push(nextBlock);
        eventBus.broadcast({ type: "message_content_added", sessionId, id: message.id, block: nextBlock });
        return;
    }
    block.text += delta;
    eventBus.broadcast({ type: "message_content_delta", sessionId, id: message.id, blockId: block.id, blockType: type, delta });
};

export const setAgentActivity = ({ eventBus, state }, sessionId, activity) => {
    const context = state.sessionContexts.get(sessionId);
    if (!context) return;
    let message = context.messages[context.messages.length - 1];
    if (!message || message.role !== "assistant") {
        if (activity === undefined) return;
        message = { id: crypto.randomUUID(), role: "assistant", content: [] };
        context.messages.push(message);
        eventBus.broadcast({ type: "message_added", sessionId, message });
    }
    message.activity = activity;
    eventBus.broadcast({ type: "message_activity", sessionId, id: message.id, activity });
};

export const upsertToolCall = ({ eventBus, state }, sessionId, toolCall) => {
    if (!toolCall?.toolCallId) return;
    const message = ensureMessage({ eventBus, state }, sessionId, "assistant");
    if (!message) return;
    const block = message.content.find((item) => item.type === "tool" && item.toolCall.toolCallId === toolCall.toolCallId);
    const nextToolCall = {
        ...(block?.toolCall ?? { name: "Tool", status: "in_progress", startedAt: Date.now() }),
        ...definedToolCallFields(toolCall),
    };
    if (nextToolCall.status !== "in_progress" && nextToolCall.durationMs === undefined && nextToolCall.startedAt) {
        nextToolCall.durationMs = Date.now() - nextToolCall.startedAt;
    }
    if (block) block.toolCall = nextToolCall;
    else {
        message.content.push({ id: toolCall.toolCallId, type: "tool", toolCall: nextToolCall });
        eventBus.broadcast({ type: "message_content_added", sessionId, id: message.id, block: message.content.at(-1) });
    }
    eventBus.broadcast({ type: "message_tool_call", sessionId, id: message.id, toolCall: nextToolCall });
};

const ensureMessage = ({ eventBus, state }, sessionId, role) => {
    const context = state.sessionContexts.get(sessionId);
    if (!context) return undefined;
    let message = context.messages[context.messages.length - 1];
    if (!message || message.role !== role) {
        message = { id: crypto.randomUUID(), role, content: [] };
        context.messages.push(message);
        eventBus.broadcast({ type: "message_added", sessionId, message });
    }
    return message;
};

const definedToolCallFields = (toolCall) => Object.fromEntries(
    Object.entries(toolCall).filter(([, value]) => value !== undefined && value !== ""),
);

export const appendContentBlock = (serviceContext, sessionId, role, block, type = "text") => {
    if (shouldSkipAgentContent(block)) return;

    const text = textFromContentBlock(block);
    if (text) appendChunk(serviceContext, sessionId, role, type, text);
};

const textFromContentBlock = (block) => {
    if (!block) return "";
    if (typeof block === "string") return block;
    if (typeof block.text === "string") return block.text;
    if (block.type === "text" && typeof block.text === "string") return block.text;
    if (block.type === "resource_link" && typeof block.uri === "string") return block.uri;
    if (block.type === "resource" && typeof block.resource?.text === "string") return block.resource.text;
    return "";
};

const shouldSkipAgentContent = (block) => {
    if (!block) return false;
    if (block.type === "resource") return String(block.resource?.uri ?? "").startsWith("insight-");
    if (block.type === "text") return textHeadEchoesSystemContext(block.text);
    return false;
};

const SYSTEM_CONTEXT_URI_PATTERN = /insight-(?:hidden-context|system-prompt):\/\/project/;
const textHeadEchoesSystemContext = (text) => SYSTEM_CONTEXT_URI_PATTERN.test(String(text ?? "").slice(0, 200));

export const setLocalTitle = (state, sessionId, title) => {
    if (!sessionId || !title) return;
    const trimmed = title.trim().slice(0, 80);
    if (!trimmed) return;
    state.localTitles.set(sessionId, trimmed);
    state.sessions = state.sessions.map((session) =>
        session.sessionId === sessionId ? { ...session, title: trimmed } : session,
    );
};
