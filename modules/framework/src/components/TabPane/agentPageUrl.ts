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
import type { AgentReportReference } from '@insight/lib/AgentReport';
import { ACP_SESSION_SRC } from '../../moduleConfig';
import { ACP_CAPABILITY_TOKEN, ACP_NODE_VERSION, ACP_PORT, ACP_STATUS, ACP_STARTUP_STATUS_URL, JUPYTERLABPROXY } from '../../centralServer/websocket/defs';

export const agentPageUrl = (report?: AgentReportReference): string => {
    const params = new URLSearchParams({ acpStatus: ACP_STATUS });
    if ((ACP_STATUS === 'ready' || ACP_STATUS === 'starting') && ACP_PORT) {
        params.set('acpPort', String(ACP_PORT));
        params.set('capabilityToken', ACP_CAPABILITY_TOKEN);
    }
    if (ACP_NODE_VERSION) params.set('acpNodeVersion', ACP_NODE_VERSION);
    if (ACP_STARTUP_STATUS_URL) params.set('acpStartupStatusUrl', ACP_STARTUP_STATUS_URL);
    if (JUPYTERLABPROXY) params.set('jupyterlabProxy', 'true');
    if (report) {
        params.set('view', 'report');
        params.set('reportId', report.artifactId);
        params.set('reportVersion', String(report.version));
    }
    return `${ACP_SESSION_SRC}${ACP_SESSION_SRC.includes('?') ? '&' : '?'}${params.toString()}`;
};
