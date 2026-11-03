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
export const PUBLISH_REPORT_CAPABILITY = Object.freeze({
    name: "publish_report",
    description: "Create or update an interactive HTML report, chart or visualization in Insight's main-area report tab. Provide a complete self-contained HTML document with inline CSS/JavaScript and embedded data snapshots. Use SVG or Canvas for charts. Network requests, external libraries, frames, forms, popups and access to the parent app are blocked; do not use CDNs or fetch APIs. Use responsive layouts. Clearly label sample data and distinguish evidence from inference. Do not include secrets. Return the whole updated document and the previous artifactId/baseVersion when revising a report; set both to null for a new report. The UI displays a report card automatically, so do not repeat the HTML in chat. Keep the returned artifactId/version for future revisions.",
    inputSchema: {
        type: "object",
        properties: {
            title: { type: "string", minLength: 1, maxLength: 120 },
            description: { type: ["string", "null"], maxLength: 500 },
            html: { type: "string", minLength: 1, maxLength: 524288 },
            artifactId: { type: ["string", "null"], description: "Set null for a NEW report. For updates use the exact ID returned by the earlier publish_report call. Never invent an ID or use an empty string." },
            baseVersion: { type: ["integer", "null"], minimum: 1, description: "Set null for a NEW report. For updates use the previously returned version." },
        },
        required: ["title", "html", "artifactId", "baseVersion"], additionalProperties: false,
    },
});

export const createArtifactCapability = ({ artifactService }) => ({
    ...PUBLISH_REPORT_CAPABILITY,
    execute: (input, context) => artifactService.publish(input, context),
});
