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

export const createToolRegistry = ({ tools = [] } = {}) => {
    const registry = new Map();

    const register = (tool) => {
        if (!tool?.name || typeof tool.execute !== "function") throw new Error("invalid tool");
        if (registry.has(tool.name)) throw new Error(`tool is already registered: ${tool.name}`);
        registry.set(tool.name, tool);
        return tool;
    };

    for (const tool of tools) register(tool);

    return {
        register,
        list() {
            return [...registry.values()].map(({ execute: _execute, ...tool }) => tool);
        },
        listOpenAITools() {
            return [...registry.values()].map((tool) => ({
                type: "function",
                function: {
                    name: tool.name,
                    description: tool.description ?? "",
                    parameters: tool.inputSchema ?? { type: "object", properties: {} },
                },
            }));
        },
        async execute(name, input, context) {
            const tool = registry.get(String(name ?? ""));
            if (!tool) throw new Error(`tool is unavailable: ${name}`);
            return tool.execute(input ?? {}, context ?? {});
        },
    };
};
