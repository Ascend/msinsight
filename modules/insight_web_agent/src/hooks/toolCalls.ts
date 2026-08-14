/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import type { ChatMessage } from '../types';

export const upsertToolCall = (
    toolCalls: ChatMessage['toolCalls'] = [],
    toolCall: NonNullable<ChatMessage['toolCalls']>[number],
): NonNullable<ChatMessage['toolCalls']> => {
    const index = toolCalls.findIndex((item) => item.toolCallId === toolCall.toolCallId);
    if (index === -1) return [...toolCalls, toolCall];
    return toolCalls.map((item, itemIndex) => itemIndex === index ? { ...item, ...toolCall } : item);
};
