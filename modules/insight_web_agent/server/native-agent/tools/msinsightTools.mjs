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

const DEFAULT_BASE_URL = "http://127.0.0.1:9090";
const DEFAULT_OBSERVE_TIMEOUT_MS = 5000;

export const createMsinsightTools = ({ baseUrl = process.env.INSIGHT_WEB_AGENT_BASE_URL ?? DEFAULT_BASE_URL } = {}) => [
    {
        name: "msinsight_observe",
        description: "Observe the current MindStudio Insight page state.",
        inputSchema: {
            type: "object",
            properties: {},
            additionalProperties: false,
        },
        async execute() {
            return readPageObservation(baseUrl);
        },
    },
    {
        name: "msinsight_listActions",
        description: "List actions currently advertised by the Insight page observation.",
        inputSchema: {
            type: "object",
            properties: {},
            additionalProperties: false,
        },
        async execute() {
            const { observation } = await readPageObservation(baseUrl);
            return { actions: observation?.availableActions ?? [] };
        },
    },
    {
        name: "msinsight_invokeAction",
        description: "Request approval before invoking a semantic Insight page action.",
        inputSchema: {
            type: "object",
            properties: {
                actionId: { type: "string" },
                payload: { type: "object" },
            },
            required: ["actionId"],
            additionalProperties: false,
        },
        async execute(input) {
            const actionId = String(input?.actionId ?? "").trim();
            if (!actionId) throw new Error("actionId is required");
            return { status: "approval_required", actionId };
        },
    },
];

const readPageObservation = async (baseUrl) => {
    try {
        const timeoutMs = Number(process.env.MSINSIGHT_OBSERVE_TIMEOUT_MS ?? DEFAULT_OBSERVE_TIMEOUT_MS);
        const response = await fetch(new URL("/api/page/observation", baseUrl), {
            signal: AbortSignal.timeout(timeoutMs),
        });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const body = await response.json();
        return {
            observation: body.observation ?? null,
            updatedAt: body.updatedAt ?? null,
            stale: !body.observation,
            message: body.observation ? undefined : "No Insight page observation has been received yet.",
        };
    } catch (error) {
        return {
            observation: null,
            updatedAt: null,
            stale: true,
            error: error.message,
            message: "Failed to read Insight page observation from insight_web_agent server.",
        };
    }
};
