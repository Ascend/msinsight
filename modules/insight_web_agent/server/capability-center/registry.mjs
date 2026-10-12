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

// Registry 是协议无关的能力目录，不感知 ACP Session、MCP transport 或前端 Command 路由。
export const createCapabilityRegistry = ({ capabilities = [] } = {}) => {
    const registry = new Map();

    const register = (capability) => {
        const name = String(capability?.name ?? "").trim();
        if (!name || typeof capability?.execute !== "function") throw capabilityError("CAPABILITY_INVALID", "Capability name and execute function are required.");
        if (!capability.inputSchema || capability.inputSchema.type !== "object") throw capabilityError("CAPABILITY_INVALID", `Capability '${name}' must define an object input schema.`);
        if (registry.has(name)) throw capabilityError("CAPABILITY_CONFLICT", `Capability '${name}' is already registered.`);
        // 对外只暴露模型可见定义，execute/validate 始终留在 Host 进程内。
        const definition = Object.freeze({
            name,
            description: String(capability.description ?? ""),
            inputSchema: capability.inputSchema,
            ...(capability.outputSchema ? { outputSchema: capability.outputSchema } : {}),
            ...(capability.annotations ? { annotations: capability.annotations } : {}),
        });
        registry.set(name, Object.freeze({ definition, execute: capability.execute, validate: capability.validate }));
        return definition;
    };

    for (const capability of capabilities) register(capability);

    return {
        register,
        list() {
            return [...registry.values()].map(({ definition }) => definition);
        },
        async execute(name, input, context) {
            const capability = registry.get(String(name ?? ""));
            if (!capability) throw capabilityError("CAPABILITY_NOT_FOUND", `Capability '${name}' is unavailable.`);
            const normalizedInput = input ?? {};
            capability.validate?.(normalizedInput);
            return capability.execute(normalizedInput, context ?? {});
        },
    };
};

export const capabilityError = (code, message, retryable = false, details) => Object.assign(new Error(message), {
    code,
    retryable,
    ...(details === undefined ? {} : { details }),
});
