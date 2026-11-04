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
import { randomUUID } from "node:crypto";
import { MSINSIGHT_CAPABILITY } from "./definitions.mjs";
import { createRagCapability } from "./ragCapability.mjs";
import { createArtifactCapability } from "./artifactCapability.mjs";
import { capabilityError, createCapabilityRegistry } from "./registry.mjs";
import { createToolError, inheritToolErrorContext, isToolDiagnosticId, normalizeToolError } from "../utils/toolError.mjs";
import { readDiagnosticProperty } from "../utils/toolErrorBudget.mjs";

// Explicit capability → diagnostic source mapping; unknown/future capabilities fall back to "runtime"
// (already part of TOOL_ERROR_SOURCES) instead of being mislabeled as CLI.
const SOURCE_BY_NAME = Object.freeze({ msinsight: "command", rag_retrieve: "rag" });

// 能力中心是所有 Adapter 的统一执行入口；HTTP MCP 与 Native Tool 不得各自维护业务实现。
export const createCapabilityCenter = ({ frontendCommandService, ragService, artifactService, logger = console } = {}) => {
    const registry = createCapabilityRegistry();
    registry.register({
        ...MSINSIGHT_CAPABILITY,
        validate(input) {
            if (!input || typeof input !== "object" || Array.isArray(input)) {
                throw capabilityError("CAPABILITY_INVALID_ARGUMENT", "input must be an object.");
            }
            const unknownFields = Object.keys(input).filter((key) => !["command", "args"].includes(key));
            if (unknownFields.length) throw capabilityError("CAPABILITY_INVALID_ARGUMENT", `Unknown input field '${unknownFields[0]}'.`);
            const command = typeof input.command === "string" ? input.command.trim() : "";
            if (!command) throw capabilityError("CAPABILITY_INVALID_ARGUMENT", "command is required.");
            if (input.args !== undefined && (!input.args || typeof input.args !== "object" || Array.isArray(input.args))) {
                throw capabilityError("CAPABILITY_INVALID_ARGUMENT", "args must be an object.");
            }
        },
        async execute(input, context) {
            // msinsight 只负责把稳定 Tool 调用路由到既有 Frontend Command Broker。
            return frontendCommandService.request({
                requestId: context.invocationId,
                diagnosticId: context.diagnosticId,
                sessionId: context.sessionId,
                command: String(input.command).trim(),
                args: input?.args ?? {},
                signal: context.signal,
            });
        },
    });
    registry.register(createRagCapability({ ragService, logger }));
    if (artifactService) registry.register(createArtifactCapability({ artifactService }));

    // MCP 调用当前没有可信 ACP Session ID，因此 sessionId/agentInstanceId 只是可选上下文，不能作为安全边界。
    const invoke = async ({ invocationId = randomUUID(), name, input, sessionId, agentInstanceId, signal } = {}) => {
        const context = {
            invocationId: String(invocationId),
            diagnosticId: isToolDiagnosticId(invocationId) ? invocationId : randomUUID(),
            toolName: name,
            source: SOURCE_BY_NAME[name] ?? "runtime",
            stage: name === "rag_retrieve" ? "retrieve" : "execute",
            outcome: "unknown", retryAdvice: "inspect_before_retry",
            sessionId: String(sessionId ?? ""), agentInstanceId: String(agentInstanceId ?? ""), signal,
        };
        try {
            return await registry.execute(name, input, context);
        } catch (error) {
            const code = readDiagnosticProperty(error, "code");
            const validation = ["CAPABILITY_INVALID_ARGUMENT", "CAPABILITY_NOT_FOUND", "COMMAND_INVALID", "COMMAND_NOT_FOUND"].includes(code);
            const busy = ["CLI_BUSY", "COMMAND_BUSY", "RAG_BUSY", "TABLE_BUSY", "CAPABILITY_CONCURRENT"].includes(code);
            const denied = ["COMMAND_PERMISSION_DENIED", "CAPABILITY_DENIED_BY_USER"].includes(code);
            const reason = readDiagnosticProperty(readDiagnosticProperty(error, "details"), "reason");
            const refresh = code === "TABLE_STATE_STALE" || (code === "COMMAND_UNAVAILABLE"
                && ["module_not_active", "module_not_registered", "command_not_registered"].includes(reason));
            const fallback = validation
                ? { ...context, stage: "validate", outcome: "not_started", retryAdvice: "correct_input" }
                : busy ? { ...context, outcome: "not_started", retryAdvice: "retry_after_ready" }
                : denied ? { ...context, stage: "authorize", outcome: "not_started", retryAdvice: "do_not_retry" }
                : refresh ? { ...context, stage: "validate", outcome: "not_started", retryAdvice: "refresh_context" }
                : context;
            const shape = normalizeToolError(error, inheritToolErrorContext(error, fallback));
            try {
                logger.warn?.(JSON.stringify({ component: "capability-center", event: "tool_error", error: shape }));
            } catch {
                // Diagnostic IO must not replace the original tool outcome.
            }
            throw createToolError(shape, error);
        }
    };

    return { invoke, list: registry.list, register: registry.register };
};
