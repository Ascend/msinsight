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
import React, { type RefObject, useEffect, useMemo, useRef, useState } from 'react';
import { Resizer } from '@insight/lib';

import type { Session } from '@/entity/session';
import { ACP_SESSION_SRC } from '@/moduleConfig';
import { ACP_PORT, JUPYTERLABPROXY } from '@/centralServer/websocket/defs';

export const ACP_SESSION_MIN_WIDTH = 500;

interface WebAgentSessionPanelProps {
    activeModule: string;
    moduleFrameMinWidth: number;
    session: Session;
    show: boolean;
    tabBodyRef: RefObject<HTMLDivElement>;
}

export const WebAgentSessionPanel = ({ activeModule, moduleFrameMinWidth, session, show, tabBodyRef }: WebAgentSessionPanelProps): JSX.Element => {
    const [sessionPanelWidth, setSessionPanelWidth] = useState(ACP_SESSION_MIN_WIDTH);
    const [acpSessionReady, setAcpSessionReady] = useState(false);
    const acpSessionFrameRef = useRef<HTMLIFrameElement>(null);

    const acpSessionSrc = useMemo(() => {
        const acpSessionParams = new URLSearchParams({
            acpPort: String(ACP_PORT),
        });
        if (JUPYTERLABPROXY) {
            acpSessionParams.set('jupyterlabProxy', 'true');
        }
        return `${ACP_SESSION_SRC}${ACP_SESSION_SRC.includes('?') ? '&' : '?'}${acpSessionParams.toString()}`;
    }, []);

    const resizeSessionPanel = (moveWidthLength: number): void => {
        const bodyWidth = tabBodyRef.current?.clientWidth ?? window.innerWidth;
        const maxWidth = Math.max(ACP_SESSION_MIN_WIDTH, bodyWidth - moduleFrameMinWidth);
        setSessionPanelWidth((current) => Math.min(maxWidth, Math.max(ACP_SESSION_MIN_WIDTH, current - moveWidthLength)));
    };

    const sendAcpSessionContext = (): void => {
        const targetWindow = acpSessionFrameRef.current?.contentWindow;
        if (!targetWindow) return;
        targetWindow.postMessage({
            event: 'insightWebAgent/context',
            body: {
                profileId: session.activeDataSource.projectName ?? '',
                activeModule,
            },
        }, '*');
    };

    useEffect(() => {
        const handleMessage = (event: MessageEvent<{ event?: string }>): void => {
            if (event.source !== acpSessionFrameRef.current?.contentWindow) return;
            if (event.data?.event === 'insightWebAgent/ready') setAcpSessionReady(true);
        };
        window.addEventListener('message', handleMessage);
        return () => window.removeEventListener('message', handleMessage);
    }, []);

    useEffect(() => {
        if (!acpSessionReady) return;
        sendAcpSessionContext();
    }, [acpSessionReady, session.activeDataSource.projectName, activeModule]);

    return <div className="acp-session-wrapper" style={{ display: show ? 'block' : 'none', width: sessionPanelWidth }}>
        <Resizer
            callback={resizeSessionPanel}
            style={{ left: 0, top: 16, height: 'calc(100% - 16px)', zIndex: 1 }}
        />
        <iframe
            ref={acpSessionFrameRef}
            className="acp-session-panel"
            id="AcpSession"
            name="AcpSession"
            src={acpSessionSrc}
            title="ACP Session"
        />
    </div>;
};
