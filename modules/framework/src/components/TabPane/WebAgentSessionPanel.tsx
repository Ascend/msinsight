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
import React, { type RefObject, useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useTheme } from '@emotion/react';
import { Resizer } from '@insight/lib';
import { ACP_MESSAGE_CHANNEL } from '@insight/lib/FrontendAgentCommand';
import {
    frameWindowMessageOrigin,
    getWindowMessageRouter,
    recordWindowMessageDebug,
    isWindowMessageChannel,
    matchesWindowMessageOrigin,
    withWindowMessageChannel,
} from '@insight/lib/WindowMessageRouter';

import type { ModuleConfig } from '@/moduleConfig';
import type { Session } from '@/entity/session';
import { ACP_SESSION_SRC } from '@/moduleConfig';
import { ACP_CAPABILITY_TOKEN, ACP_PORT, JUPYTERLABPROXY } from '@/centralServer/websocket/defs';
import { frontendAgentCommandController } from '@/agent/frontendAgentCommandController';

export const ACP_SESSION_MIN_WIDTH = 500;

interface WebAgentSessionPanelProps {
    activeModule: string;
    availableModules: ModuleConfig[];
    moduleFrameMinWidth: number;
    onRequestClose: () => void;
    session: Session;
    show: boolean;
    tabBodyRef: RefObject<HTMLDivElement>;
}

export const WebAgentSessionPanel = ({ activeModule, availableModules, moduleFrameMinWidth, onRequestClose, session, show, tabBodyRef }: WebAgentSessionPanelProps): JSX.Element => {
    const theme = useTheme();
    const [sessionPanelWidth, setSessionPanelWidth] = useState(ACP_SESSION_MIN_WIDTH);
    const acpSessionFrameRef = useRef<HTMLIFrameElement>(null);
    const acpSessionReadyRef = useRef(false);

    const acpSessionSrc = useMemo(() => {
        const acpSessionParams = new URLSearchParams({
            acpPort: String(ACP_PORT),
            capabilityToken: ACP_CAPABILITY_TOKEN,
        });
        if (JUPYTERLABPROXY) {
            acpSessionParams.set('jupyterlabProxy', 'true');
        }
        return `${ACP_SESSION_SRC}${ACP_SESSION_SRC.includes('?') ? '&' : '?'}${acpSessionParams.toString()}`;
    }, []);
    const acpSessionOrigin = useMemo(() => frameWindowMessageOrigin(acpSessionSrc), [acpSessionSrc]);

    const resizeSessionPanel = (moveWidthLength: number): void => {
        const bodyWidth = tabBodyRef.current?.clientWidth ?? window.innerWidth;
        const maxWidth = Math.max(ACP_SESSION_MIN_WIDTH, bodyWidth - moduleFrameMinWidth);
        setSessionPanelWidth((current) => Math.min(maxWidth, Math.max(ACP_SESSION_MIN_WIDTH, current - moveWidthLength)));
    };

    const sendAcpSessionContext = useCallback((): void => {
        const targetWindow = acpSessionFrameRef.current?.contentWindow;
        if (!targetWindow) return;
        const contextMessage = withWindowMessageChannel(ACP_MESSAGE_CHANNEL, {
            event: 'insightWebAgent/context',
            body: {
                profileId: session.activeDataSource.projectName ?? '',
                activeModule,
            },
        });
        recordWindowMessageDebug({
            direction: 'outbound',
            data: contextMessage,
            origin: window.location.origin,
            target: 'AcpSession',
        });
        targetWindow.postMessage(contextMessage, acpSessionOrigin);
    }, [session.activeDataSource.projectName, activeModule, acpSessionOrigin]);

    useEffect(() => {
        const frame = acpSessionFrameRef.current;
        if (!frame) return;
        return frontendAgentCommandController.attachAgentFrame(frame);
    }, [acpSessionOrigin]);

    useEffect(() => frontendAgentCommandController.setFrameworkObservationProvider(() => ({
        activeModule,
        availableModules: availableModules.map(module => module.name),
        scene: session.scene,
        loading: session.loading,
        selectedProjectName: session.activeDataSource.projectName,
        hasSelectedFile: Boolean(session.activeDataSource.selectedFilePath),
        selectedFileType: session.activeDataSource.selectedFileType,
        clusterPageInfo: {
            selectedClusterPath: session.clusterPageInfo.selectedClusterPath,
            clusterCount: session.clusterPageInfo.clusterList.length,
        },
        timelinePageInfo: {
            unitCount: session.timelinePageInfo.unitCount,
        },
    })), [activeModule, availableModules, session]);

    useEffect(() => {
        const handleAgentEvent = (event: MessageEvent): void => {
            if (event.source !== acpSessionFrameRef.current?.contentWindow || !matchesWindowMessageOrigin(event.origin, acpSessionOrigin)) return;
            const eventName = (event.data as { event?: string })?.event;
            if (eventName === 'insightWebAgent/ready') {
                acpSessionReadyRef.current = true;
                sendAcpSessionContext();
            } else if (eventName === 'insightWebAgent/close') {
                onRequestClose();
            }
        };
        return getWindowMessageRouter().subscribe(handleAgentEvent, isWindowMessageChannel(ACP_MESSAGE_CHANNEL));
    }, [acpSessionOrigin, onRequestClose, sendAcpSessionContext]);

    useEffect(() => {
        if (acpSessionReadyRef.current) sendAcpSessionContext();
    }, [sendAcpSessionContext]);

    return <div className="acp-session-wrapper" style={{ display: show ? 'block' : 'none', width: sessionPanelWidth }}>
        <Resizer
            callback={resizeSessionPanel}
            style={{
                left: -8,
                top: '50%',
                width: 3,
                height: 48,
                zIndex: 2,
                borderRadius: 2,
                background: theme.scrollbarColor,
                cursor: 'col-resize',
                transform: 'translate(-50%, -50%)',
            }}
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
