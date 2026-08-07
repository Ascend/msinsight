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
export const appendChunk = ({ eventBus, state }, sessionId, role, field, delta) => {
    const context = state.sessionContexts.get(sessionId);
    if (!context) return;
    let message = context.messages[context.messages.length - 1];
    if (!message || message.role !== role) {
        message = role === "assistant"
            ? { id: crypto.randomUUID(), role, text: "", thinking: "" }
            : { id: crypto.randomUUID(), role, text: "" };
        context.messages.push(message);
        eventBus.broadcast({ type: "message_added", sessionId, message });
    }

    message[field] = `${message[field] ?? ""}${delta}`;
    eventBus.broadcast({ type: "message_delta", sessionId, id: message.id, field, delta });
};

export const appendContentBlock = (serviceContext, sessionId, role, block, field = "text") => {
    if (isHiddenContextBlock(block)) return;

    const text = textFromContentBlock(block);
    if (text) appendChunk(serviceContext, sessionId, role, field, text);

    const image = imageFromContentBlock(block);
    if (!image) return;

    const context = serviceContext.state.sessionContexts.get(sessionId);
    if (!context) return;
    let message = context.messages[context.messages.length - 1];
    if (!message || message.role !== role) {
        message = role === "assistant"
            ? { id: crypto.randomUUID(), role, text: "", thinking: "", images: [] }
            : { id: crypto.randomUUID(), role, text: "", images: [] };
        context.messages.push(message);
        serviceContext.eventBus.broadcast({ type: "message_added", sessionId, message });
    }
    message.images = [...(message.images ?? []), image];
    serviceContext.eventBus.broadcast({ type: "message_delta", sessionId, id: message.id, field: "images", delta: [image] });
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

const isHiddenContextBlock = (block) => {
    if (!block || block.type !== "resource") return false;
    return String(block.resource?.uri ?? "").startsWith("insight-hidden-context://");
};

const imageFromContentBlock = (block) => {
    if (!block || block.type !== "image") return undefined;
    const data = String(block.data ?? "").trim();
    const mimeType = String(block.mimeType ?? block.mime_type ?? "").trim();
    if (!data || !mimeType.startsWith("image/")) return undefined;
    return {
        id: crypto.randomUUID(),
        name: imageNameFromUri(block.uri),
        data,
        mimeType,
    };
};

const imageNameFromUri = (uri) => {
    try {
        return new URL(uri).searchParams.get("name") || "image";
    } catch (_error) {
        return "image";
    }
};

export const setLocalTitle = (state, sessionId, title) => {
    if (!sessionId || !title) return;
    const trimmed = title.trim().slice(0, 80);
    if (!trimmed) return;
    state.localTitles.set(sessionId, trimmed);
    state.sessions = state.sessions.map((session) =>
        session.sessionId === sessionId ? { ...session, title: trimmed } : session,
    );
};
