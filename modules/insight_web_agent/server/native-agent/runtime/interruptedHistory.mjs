/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */

// Keep raw model data separate from display blocks, whose tool values are truncated.
// Only complete tool-call events are replayable; streamed input fragments are not calls.
export const createInterruptedHistory = () => {
    const messages = [];
    let assistant = [];
    let results = new Map();
    const flush = () => {
        if (!assistant.length) return;
        messages.push({ role: "assistant", content: assistant });
        const calls = assistant.filter((part) => part.type === "tool-call");
        if (calls.length) messages.push({ role: "tool", content: calls.map((call) => ({
            type: "tool-result", toolCallId: call.toolCallId, toolName: call.toolName,
            output: results.get(call.toolCallId) ?? {
                type: "error-text",
                value: "Turn interrupted before a final tool result was received. Side effects may have occurred; observe the current state before retrying.",
            },
        })) });
        assistant = [];
        results = new Map();
    };
    return {
        event(event) {
            if (event.type === "start-step" || event.type === "finish-step") flush();
            if (event.type === "text-delta") {
                const last = assistant.at(-1);
                if (last?.type === "text") last.text += event.text;
                else assistant.push({ type: "text", text: event.text });
            }
            if (event.type === "tool-call") assistant.push({
                type: "tool-call", toolCallId: event.toolCallId, toolName: event.toolName, input: event.input,
                ...(event.providerMetadata ? { providerOptions: event.providerMetadata } : {}),
            });
            // Preliminary results cannot close a call; abort may happen before its final output.
            if (event.type === "tool-result" && !event.preliminary) results.set(event.toolCallId,
                typeof event.output === "string" ? { type: "text", value: event.output } : { type: "json", value: event.output ?? null });
            if (event.type === "tool-error") results.set(event.toolCallId, {
                type: "error-text", value: event.error instanceof Error ? event.error.message : String(event.error),
            });
        },
        finish() { flush(); return messages; },
    };
};
