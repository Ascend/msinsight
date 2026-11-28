/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const unboundMessages = new WeakSet();

export const findHistoryTool = (messages, toolCallId) => {
    for (let index = messages.length - 1; index >= 0; index--) {
        const message = messages[index];
        const block = message.content.find((part) => part.type === "tool" && part.toolCall?.toolCallId === toolCallId);
        if (block) {
            return { message, block };
        }
    }
    return undefined;
};

/** Only use while collecting replay; live message identities must remain stable. */
export const ensureHistoryMessage = (messages, role, id) => {
    const sourceId = typeof id === "string" && id ? id : undefined;
    const last = messages.at(-1);
    if (sourceId) {
        const existing = last?.role === role && last.id === sourceId ? last
            : messages.find((message) => message.role === role && message.id === sourceId);
        if (existing) {
            return existing;
        }
        if (last?.role === role && unboundMessages.has(last)) {
            last.id = sourceId;
            unboundMessages.delete(last);
            return last;
        }
    } else if (last?.role === role) {
        return last;
    }
    const message = { id: sourceId ?? crypto.randomUUID(), role, content: [] };
    if (!sourceId) {
        unboundMessages.add(message);
    }
    messages.push(message);
    return message;
};
