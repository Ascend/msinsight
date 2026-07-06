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
import { appendFile, mkdir } from "node:fs/promises";
import { dirname, join } from "node:path";

export const createAuditLogger = ({ cwd, debug }) => {
    const logPath = join(cwd, "audit.log");
    const write = async (event) => {
        const line = JSON.stringify({ ts: Date.now(), ...event }) + "\n";
        if (debug) console.error(`[audit] ${line.trim()}`);
        try {
            await mkdir(dirname(logPath), { recursive: true });
            await appendFile(logPath, line, "utf8");
        } catch (error) {
            console.warn(`Failed to write audit log: ${error.message}`);
        }
    };
    return {
        sessionStart: (ctx) => write({ kind: "session_start", sessionId: ctx.sessionId, agentId: ctx.agentId, mode: ctx.mode }),
        sessionEnd: (sid) => write({ kind: "session_end", sessionId: sid }),
        contextAssembled: (sid, packet) => write({ kind: "context_assembled", sessionId: sid, providerCount: packet.contextProviders.length }),
        toolCall: (sid, capability, argsHash) => write({ kind: "tool_call", sessionId: sid, capability, argsHash, bus: "mcp" }),
        toolResult: (sid, capability, ok) => write({ kind: "tool_result", sessionId: sid, capability, ok }),
        error: (sid, err) => write({ kind: "error", sessionId: sid, errorCode: err?.code ?? "UNKNOWN", message: err?.message ?? String(err) }),
    };
};
