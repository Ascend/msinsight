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

/** 功能：创建 ACP JSON-RPC 输出器和 session/update 通知发送器。 */
export const createAcpSessionNotifier = (output = process.stdout) => ({
    writeJson: (value) => writeJson(output, value),
    sendSessionChunk: (sessionId, text) => sendSessionContentChunk(output, sessionId, "agent_message_chunk", text),
    sendSessionThinkingChunk: (sessionId, text) => sendSessionContentChunk(output, sessionId, "agent_thought_chunk", text),
    sendSessionContentChunk: (sessionId, kind, text) => sendSessionContentChunk(output, sessionId, kind, text),
    sendAgentActivityUpdate: (sessionId, activity) => sendAgentActivityUpdate(output, sessionId, activity),
    sendToolCallUpdate: (sessionId, kind, toolCall) => sendToolCallUpdate(output, sessionId, kind, toolCall),
});

/** 功能：把 JSON-RPC 对象序列化为单行 JSON 并写入 ACP stdout。 */
const writeJson = (output, value) => {
    output.write(`${JSON.stringify(value)}\n`);
};

/** 功能：构造并输出指定类型的 ACP session/update 文本通知。 */
const sendSessionContentChunk = (output, sessionId, kind, text) => {
    writeJson(output, {
        jsonrpc: "2.0",
        method: "session/update",
        params: {
            sessionId,
            update: {
                kind,
                content: { type: "text", text },
            },
        },
    });
};

/** 功能：构造并输出 agent_status_update 活动状态通知。 */
const sendAgentActivityUpdate = (output, sessionId, activity) => {
    writeJson(output, {
        jsonrpc: "2.0",
        method: "session/update",
        params: {
            sessionId,
            update: {
                kind: "agent_status_update",
                activity,
            },
        },
    });
};

/** 功能：构造并输出工具开始或更新通知，包含输入、进度、输出和耗时。 */
const sendToolCallUpdate = (output, sessionId, kind, toolCall) => {
    writeJson(output, {
        jsonrpc: "2.0",
        method: "session/update",
        params: {
            sessionId,
            update: {
                kind,
                toolCallId: toolCall.toolCallId,
                title: toolCall.name,
                status: toolCall.status,
                rawInput: toolCall.input,
                content: toolCall.progress,
                rawOutput: toolCall.output,
                startedAt: toolCall.startedAt,
                durationMs: toolCall.durationMs,
            },
        },
    });
};
