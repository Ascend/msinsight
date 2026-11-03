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
import type { ToolCallItem } from './types';

export interface ReportArtifact {
    kind: 'visualization';
    artifactId: string;
    version: number;
    title: string;
    description: string;
    createdAt: string;
}

export interface ReportDocument extends ReportArtifact {
    html: string;
    versions: ReportArtifact[];
}

const findArtifact = (value: unknown, depth = 0): ReportArtifact | undefined => {
    if (depth > 8 || !value) return undefined;
    if (typeof value === 'string') {
        try { return findArtifact(JSON.parse(value), depth + 1); } catch { return undefined; }
    }
    if (Array.isArray(value)) return value.map((item) => findArtifact(item, depth + 1)).find(Boolean);
    if (typeof value !== 'object') return undefined;
    const item = value as Record<string, unknown>;
    if (item.kind === 'visualization' && typeof item.artifactId === 'string' &&
        /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/.test(item.artifactId) &&
        Number.isSafeInteger(item.version) && Number(item.version) > 0 && typeof item.title === 'string') {
        return {
            kind: 'visualization',
            artifactId: item.artifactId,
            version: Number(item.version),
            title: item.title,
            description: typeof item.description === 'string' ? item.description : '',
            createdAt: typeof item.createdAt === 'string' ? item.createdAt : '',
        };
    }
    // ACP agents may wrap MCP results in { output: "...", metadata: ... }.
    // Try each supported envelope: an empty field must not hide another valid result.
    for (const field of ['structuredContent', 'output', 'result', 'content', 'text']) {
        const report = findArtifact(item[field], depth + 1);
        if (report) return report;
    }
    return undefined;
};

export const reportFromTool = (tool: ToolCallItem): ReportArtifact | undefined => tool.status === 'completed'
    ? findArtifact(tool.output) ?? findArtifact(tool.progress)
    : undefined;

export const isReportTool = (tool: ToolCallItem): boolean => /(?:^|[_/])publish_report$/.test(tool.name) || Boolean(reportFromTool(tool));
export const reportKey = (report: ReportArtifact): string => `${report.artifactId}:${report.version}`;

// This policy precedes all untrusted markup. The opaque-origin iframe supplies the
// other half of isolation: never add allow-same-origin, forms or navigation flags.
export const reportSrcDoc = (html: string): string => `<!doctype html><html><head>
<meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src data: blob:; font-src data:; connect-src 'none'; frame-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'; worker-src 'none'">
<meta name="referrer" content="no-referrer">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>html{color-scheme:light}body{margin:0;padding:24px;font-family:system-ui,sans-serif;color:#202632;background:#fff;box-sizing:border-box}*,*:before,*:after{box-sizing:border-box}img,svg,canvas{max-width:100%}</style>
</head><body>${html}</body></html>`;
