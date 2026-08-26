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

// MCP Adapter 与 ACP Session 注入共享这些标识，避免 Tool Server 名称或路由在两侧漂移。
export const CAPABILITY_MCP_NAME = "msinsight-capabilities";
export const CAPABILITY_MCP_PATH = "/mcp/capabilities";
export const CAPABILITY_MCP_SERVER_INFO = Object.freeze({ name: "msinsight-capability-center", version: "0.1.0" });

// 模型只看到一个稳定的 msinsight Tool；页面中的动态 Command 继续由 help/observe 发现。
export const MSINSIGHT_CAPABILITY = Object.freeze({
    name: "msinsight",
    description: "Discover, observe, and operate the current MindStudio Insight page through structured commands. Use command 'help' to discover available commands.",
    inputSchema: Object.freeze({
        type: "object",
        properties: {
            command: { type: "string", minLength: 1 },
            args: { type: "object" },
        },
        required: ["command"],
        additionalProperties: false,
    }),
});
