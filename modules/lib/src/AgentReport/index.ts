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
export interface AgentReportReference {
    artifactId: string;
    version: number;
    title: string;
}

export const REPORT_OPEN = 'insightWebAgent/openReport';
export const REPORT_OPENED = 'insightWebAgent/reportOpened';
export const REPORT_READY = 'insightWebAgent/reportReady';
export const REPORT_CLOSE = 'insightWebAgent/closeReport';

// Only references cross the host boundary; never accept a URL or generated HTML.
export const parseAgentReportReference = (value: unknown): AgentReportReference | undefined => {
    if (!value || typeof value !== 'object') return undefined;
    const item = value as Record<string, unknown>;
    if (typeof item.artifactId !== 'string' || !/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/.test(item.artifactId) ||
        !Number.isSafeInteger(item.version) || Number(item.version) < 1 || typeof item.title !== 'string' || item.title.length > 120) return undefined;
    return { artifactId: item.artifactId, version: Number(item.version), title: item.title };
};

export const reportTabKey = (report: AgentReportReference): string => `report:${report.artifactId}`;
