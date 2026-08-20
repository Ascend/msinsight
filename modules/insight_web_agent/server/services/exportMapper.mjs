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
export const messagesFromExport = (exported) => {
    return (exported.messages ?? []).flatMap((message) => {
        const role = message.info?.role;
        if (role !== "user" && role !== "assistant") return [];

        const content = (message.parts ?? []).flatMap((part) => {
            if (part.type === "text") return [{ id: part.id ?? crypto.randomUUID(), type: "text", text: part.text ?? "" }];
            if (role === "assistant" && part.type === "reasoning") {
                return [{ id: part.id ?? crypto.randomUUID(), type: "thinking", text: part.text ?? "" }];
            }
            if (role === "assistant" && part.type === "tool") {
                return [{ id: part.id ?? crypto.randomUUID(), type: "tool", toolCall: toolCallFromPart(part) }];
            }
            return [];
        });
        return content.length ? [{ id: message.info?.id ?? crypto.randomUUID(), role, content }] : [];
    });
};

const toolCallFromPart = (part) => ({
    toolCallId: String(part.callID ?? part.id ?? crypto.randomUUID()),
    name: String(part.tool ?? "Tool"),
    status: part.state?.status === "completed" ? "completed" : part.state?.status === "error" ? "failed" : "in_progress",
    input: part.state?.input === undefined ? undefined : JSON.stringify(part.state.input, null, 2),
    output: part.state?.output === undefined ? undefined : String(part.state.output),
});
