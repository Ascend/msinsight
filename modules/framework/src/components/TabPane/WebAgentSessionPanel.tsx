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
import { Resizer } from '@insight/lib';
import { FrontendAgentToolBridgeServer, AGENT_TOOL_REQUEST, type ToolRequestMessage } from '@insight/lib/FrontendAgentToolBridge';

import type { ModuleConfig } from '@/moduleConfig';
import type { Session } from '@/entity/session';
import { ACP_SESSION_SRC } from '@/moduleConfig';
import { ACP_PORT, JUPYTERLABPROXY } from '@/centralServer/websocket/defs';
import { setFrontendAgentToolBridgeServer } from '@/agent/frontendAgentToolRegistry';
import { registerObserveTool } from '@/agent/tools/observe';

export const ACP_SESSION_MIN_WIDTH = 500;

interface WebAgentSessionPanelProps {
    activeModule: string;
    availableModules: ModuleConfig[];
    moduleFrameMinWidth: number;
    session: Session;
    show: boolean;
    tabBodyRef: RefObject<HTMLDivElement>;
}

export const WebAgentSessionPanel = ({ activeModule, availableModules, moduleFrameMinWidth, session, show, tabBodyRef }: WebAgentSessionPanelProps): JSX.Element => {
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

    const sendAcpSessionContext = useCallback((): void => {
        const targetWindow = acpSessionFrameRef.current?.contentWindow;
        if (!targetWindow) return;
        targetWindow.postMessage({
            event: 'insightWebAgent/context',
            body: {
                profileId: session.activeDataSource.projectName ?? '',
                activeModule,
            },
        }, '*');
    }, [session.activeDataSource.projectName, activeModule]);

    // FrontendAgentToolBridgeServer：接收 iframe 的工具调用请求，按 tool 名分发到对应 handler。
    // 不自注册 listener，由下方单一 listener 按 event 分发委托给它。
    // 工具实现通过 `frontendAgentToolRegistry` 注册，与 server 实例化解耦。
    const toolBridge = useMemo(() => new FrontendAgentToolBridgeServer({
        targetFrame: () => acpSessionFrameRef.current,
    }), []);

    // 把 server 注册到全局注册表，并注册 observe 工具（getter 注入当前状态）。
    // 工具注册随组件生命周期——WebAgentSessionPanel 卸载时工具无意义。
    useEffect(() => {
        setFrontendAgentToolBridgeServer(toolBridge);
        const unregisterObserve = registerObserveTool(() => ({
            activeModule,
            availableModules,
            session,
        }));
        return () => {
            unregisterObserve();
            setFrontendAgentToolBridgeServer(null);
            toolBridge.dispose();
        };
    }, [toolBridge, activeModule, availableModules, session]);

    // 单一 message listener：整合 ready 通知 + 工具调用请求。
    // 这是对 reviewer 意见 #1 的响应——消除原 AgentBridge.ts + WebAgentSessionPanel
    // 两个并行 listener 的结构混乱。
    useEffect(() => {
        const handleMessage = (event: MessageEvent): void => {
            if (event.source !== acpSessionFrameRef.current?.contentWindow) return;
            switch ((event.data as { event?: string })?.event) {
                case 'insightWebAgent/ready':
                    setAcpSessionReady(true);
                    break;
                case AGENT_TOOL_REQUEST:
                    toolBridge.handleMessage(event as MessageEvent<ToolRequestMessage>);
                    break;
            }
        };
        window.addEventListener('message', handleMessage);
        return () => window.removeEventListener('message', handleMessage);
    }, [toolBridge]);

    useEffect(() => {
        if (!acpSessionReady) return;
        sendAcpSessionContext();
    }, [acpSessionReady, sendAcpSessionContext]);

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
