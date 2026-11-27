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
import { filterHistoryText, isHiddenResource } from "./historyContentFilter.mjs";
import { ensureHistoryMessage } from "./historyMessageService.mjs";

export const appendChunk = (serviceContext, sessionId, role, type, delta) => {
    const { eventBus, historyBlockId } = serviceContext;
    const message = ensureMessage(serviceContext, sessionId, role);
    if (!message) {
        return;
    }
    const block = message.content.at(-1);
    if (!block || block.type !== type || (historyBlockId && block.id !== historyBlockId)) {
        const nextBlock = {
            id: historyBlockId ?? crypto.randomUUID(),
            type,
            text: delta,
            startedAt: Date.now(),
        };
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
        if (activity === undefined) {
            // 无内容的一轮会移除该消息，改按记录的 id 清除残留状态。
            const targetId = context.activityMessageId;
            if (!targetId) return;
            context.activityMessageId = undefined;
            const target = context.messages.find((item) => item.id === targetId);
            if (target) target.activity = undefined;
            eventBus.broadcast({ type: "message_activity", sessionId, id: targetId, activity: undefined });
            return;
        }
        message = { id: crypto.randomUUID(), role: "assistant", content: [] };
        context.messages.push(message);
        eventBus.broadcast({ type: "message_added", sessionId, message });
    }
    message.activity = activity;
    context.activityMessageId = activity === undefined ? undefined : message.id;
    eventBus.broadcast({ type: "message_activity", sessionId, id: message.id, activity });
};

export const upsertToolCall = (serviceContext, sessionId, toolCall) => {
    const { eventBus } = serviceContext;
    if (!toolCall?.toolCallId) {
        return;
    }
    const message = ensureMessage(serviceContext, sessionId, "assistant");
    if (!message) {
        return;
    }
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

const ensureMessage = ({ eventBus, state, historyMessageId }, sessionId, role) => {
    const context = state.sessionContexts.get(sessionId);
    if (!context) {
        return undefined;
    }
    if (context.collectingHistory) {
        return ensureHistoryMessage(context.messages, role, historyMessageId);
    }
    let message = context.messages[context.messages.length - 1];
    if (!message || message.role !== role || (historyMessageId && message.id !== historyMessageId)) {
        message = { id: historyMessageId ?? crypto.randomUUID(), role, content: [] };
        context.messages.push(message);
        eventBus.broadcast({ type: "message_added", sessionId, message });
    }
    return message;
};

const definedToolCallFields = (toolCall) => Object.fromEntries(
    Object.entries(toolCall).filter(([, value]) => value !== undefined && value !== ""),
);

export const appendContentBlock = (serviceContext, sessionId, role, block, type = "text") => {
    if (isHiddenResource(block)) {
        return;
    }
    if (block?.type === "image") {
        if (typeof block.data !== "string" || !block.data.trim() ||
            typeof block.mimeType !== "string" || !block.mimeType.startsWith("image/")) return;
        const message = ensureMessage(serviceContext, sessionId, role);
        if (!message) return;
        const image = {
            id: crypto.randomUUID(),
            type: "image",
            name: typeof block.name === "string" ? block.name : "Image",
            mimeType: block.mimeType,
            data: block.data,
        };
        message.content.push(image);
        serviceContext.eventBus.broadcast({ type: "message_content_added", sessionId, id: message.id, block: image });
        return;
    }

    const source = textFromContentBlock(block);
    const collectingHistory = serviceContext.state.sessionContexts.get(sessionId)?.collectingHistory;
    const textual = typeof block === "string" || block?.type === "text";
    const text = collectingHistory || !textual ? source : filterHistoryText(source, serviceContext.hiddenPromptTexts);
    if (text) {
        const historyBlockId = collectingHistory && typeof block?.id === "string" && block.id ? block.id : undefined;
        appendChunk({ ...serviceContext, historyBlockId }, sessionId, role, type, text);
    }
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

export const setLocalTitle = (state, sessionId, title) => {
    if (!sessionId || !title || state.customTitles?.has(sessionId)) return;
    const trimmed = title.trim().slice(0, 80);
    if (!trimmed) return;
    state.localTitles.set(sessionId, trimmed);
    state.sessions = state.sessions.map((session) =>
        session.sessionId === sessionId ? { ...session, title: trimmed } : session,
    );
};
