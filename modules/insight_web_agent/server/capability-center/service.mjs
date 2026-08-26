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
import { capabilityError, createCapabilityRegistry } from "./registry.mjs";

// 能力中心是所有 Adapter 的统一执行入口；HTTP MCP 与 Native Tool 不得各自维护业务实现。
export const createCapabilityCenter = ({ frontendCommandService } = {}) => {
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
                sessionId: context.sessionId,
                command: String(input.command).trim(),
                args: input?.args ?? {},
                signal: context.signal,
            });
        },
    });

    // MCP 调用当前没有可信 ACP Session ID，因此 sessionId/agentInstanceId 只是可选上下文，不能作为安全边界。
    const invoke = ({ invocationId = randomUUID(), name, input, sessionId, agentInstanceId, signal } = {}) => registry.execute(name, input, {
        invocationId: String(invocationId),
        sessionId: String(sessionId ?? ""),
        agentInstanceId: String(agentInstanceId ?? ""),
        signal,
    });

    return { invoke, list: registry.list, register: registry.register };
};
