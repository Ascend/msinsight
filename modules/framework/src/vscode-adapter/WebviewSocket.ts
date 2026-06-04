/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import type { ConnectHost } from './defs';
import { LOCAL_HOST } from './defs';
import { getConnectHost } from './DataViewerController';

export interface WebviewSocketCreation {
    ws: WebviewSocket;
    toIframeUrl?: string;
}
// WebviewSocket.ts
type WSLikeHandler = ((ev: any) => void) | null;

interface Outgoing {
    type: 'ws:open' | 'ws:send' | 'ws:close';
    id: string;
    payload?: any;
    url?: string;
    protocols?: string | string[];
}

interface Incoming {
    type: 'ws:opened' | 'ws:message' | 'ws:error' | 'ws:closed';
    id: string;
    payload?: any;
    reason?: string;
    code?: number;
}

export class WebviewSocket {
    static createForHost(initHost: ConnectHost): WebviewSocketCreation {
        const isVscode = typeof (window as any).acquireVsCodeApi === 'function';
        if (isVscode) {
            const host = getConnectHost();
            return { ws: new WebviewSocket(`ws://${host.remote}:${host.port}`) };
        }
        const { ws, toIframeUrl } = WebviewSocket.buildBrowserWs(initHost);
        return { ws, toIframeUrl };
    }

    private static buildBrowserWs(initHost: ConnectHost): WebviewSocketCreation {
        const protocol = `${window.location.protocol === 'https:' && window.location.host !== 'wry.localhost' ? 'wss:' : 'ws:'}//`;
        let ws: WebviewSocket;
        let toIframeUrl: string | undefined;

        if (initHost.jupyterlabProxy) {
            const { host, search } = window.location;
            const p = window.location.pathname.replace(/\/resources\/profiler\/frontend/, `/proxy/${initHost.port}/resources/profiler/frontend`);
            ws = new WebviewSocket(protocol + host + p + search);
            toIframeUrl = `${protocol}${host}${window.location.pathname.replace(/\/resources\/profiler\/frontend\/.*/, `/proxy/${initHost.port}`)}`;
        } else if (!window.location.pathname.includes('/proxy/')) {
            const hostname = window.location.hostname || LOCAL_HOST;
            let pathname = window.location.pathname && window.location.pathname !== '/' ? window.location.pathname.replace(/\/resources\/profiler\/frontend\/index.html/, '') : '';
            pathname = pathname.replace(/\/index.html/, '');
            ws = new WebviewSocket(`${protocol}${hostname}${pathname}:${initHost.port}${window.location.search}`);
            toIframeUrl = `${protocol}${hostname}${pathname}:${initHost.port}`;
        } else {
            const { location } = window;
            const { host } = location;
            const p = window.location.pathname.replace(/proxy\/\d{4}/, `proxy/${initHost.port}`);
            const { search } = location;
            ws = new WebviewSocket(protocol + host + p + search);
            toIframeUrl = `${protocol}${host}${p.replace(/\/index.html/, '')}`;
        }

        return { ws, toIframeUrl };
    }

    private readonly id = `ws-${Date.now()}-${Math.random().toString(16).slice(2)}`;
    private readonly url: string;
    private readonly protocols?: string | string[];
    private disposed = false;
    private readonly nativeWs?: WebSocket;
    private readonly useNative: boolean = false;
    private _readyState: number = WebSocket.CONNECTING;

    private _onopen: WSLikeHandler = null;
    private _onmessage: WSLikeHandler = null;
    private _onerror: WSLikeHandler = null;
    private _onclose: WSLikeHandler = null;

    get onopen(): WSLikeHandler {
        return this._onopen;
    }

    set onopen(handler: WSLikeHandler) {
        this._onopen = handler;
        if (this.useNative && this.nativeWs) this.nativeWs.onopen = handler as any;
    }

    get onmessage(): WSLikeHandler {
        return this._onmessage;
    }

    set onmessage(handler: WSLikeHandler) {
        this._onmessage = handler;
        if (this.useNative && this.nativeWs) this.nativeWs.onmessage = handler as any;
    }

    get onerror(): WSLikeHandler {
        return this._onerror;
    }

    set onerror(handler: WSLikeHandler) {
        this._onerror = handler;
        if (this.useNative && this.nativeWs) this.nativeWs.onerror = handler as any;
    }

    get onclose(): WSLikeHandler {
        return this._onclose;
    }

    set onclose(handler: WSLikeHandler) {
        this._onclose = handler;
        if (this.useNative && this.nativeWs) this.nativeWs.onclose = handler as any;
    }

    constructor(url: string, protocols?: string | string[]) {
        this.url = url;
        this.protocols = protocols;

        this.useNative = typeof (window as any).acquireVsCodeApi !== 'function';

        if (this.useNative) {
            // 非插件的 webview 环境，直接走原生 WebSocket，保证原有行为
            this.nativeWs = protocols ? new WebSocket(url, protocols) : new WebSocket(url);
            this._readyState = this.nativeWs.readyState;
            this.nativeWs.onopen = (ev) => this._onopen?.(ev);
            this.nativeWs.onmessage = (ev) => this._onmessage?.(ev);
            this.nativeWs.onerror = (ev) => this._onerror?.(ev);
            this.nativeWs.onclose = (ev) => {
                this.disposed = true;
                this._readyState = WebSocket.CLOSED;
                this._onclose?.(ev);
            };
            return;
        }

        // 插件 webview 环境：监听来自 node 的事件
        window.addEventListener('message', this.handleMessage);

        // 请求 node 打开真实 WS
        this.post({
            type: 'ws:open',
            id: this.id,
            url: this.url,
            protocols: this.protocols,
        });
    }

    send(data: string | ArrayBufferLike | Blob | ArrayBufferView): void {
        if (this.disposed) return;
        if (this.useNative && this.nativeWs) {
            this.nativeWs.send(data);
            return;
        }
        this.post({ type: 'ws:send', id: this.id, payload: data });
    }

    close(code?: number, reason?: string): void {
        if (this.disposed) return;
        this.disposed = true;
        this._readyState = WebSocket.CLOSED;
        if (this.useNative && this.nativeWs) {
            this.nativeWs.close(code, reason);
            return;
        }
        this.post({ type: 'ws:close', id: this.id, payload: { code, reason } });
        window.removeEventListener('message', this.handleMessage);
    }

    get readyState(): number {
    // 和原生 WebSocket 一致的状态机：0 CONNECTING, 1 OPEN, 2 CLOSING, 3 CLOSED
        if (this.useNative && this.nativeWs) return this.nativeWs.readyState;
        return this._readyState;
    }

    private post(msg: Outgoing): void {
    // webview 环境建议用 acquireVsCodeApi，否则用 window.parent/postMessage
        console.log('[WebviewSocket] window.vscode', (window as any).vscode);
        if (typeof (window as any).acquireVsCodeApi === 'function') {
            (window as any).vscode.postMessage(msg);
            console.log('(window as any).vscode.postMessage(msg);', msg);
        } else if (window.parent) {
            window.parent.postMessage(msg, '*');
        } else {
            window.postMessage(msg, '*');
        }
    }

    private readonly handleMessage = (event: MessageEvent<Incoming>): void => {
        const msg = event.data;
        if (!msg || msg.id !== this.id) return;

        switch (msg.type) {
            case 'ws:opened':
                this._readyState = WebSocket.OPEN;
                this.onopen?.(msg);
                break;
            case 'ws:message':
                // 后端可传 { data } 结构，这里直接透传
                this.onmessage?.({ data: msg.payload });
                break;
            case 'ws:error':
                this._readyState = WebSocket.CLOSING;
                this.onerror?.(msg);
                break;
            case 'ws:closed':
                this.disposed = true;
                this._readyState = WebSocket.CLOSED;
                this.onclose?.({ code: msg.code, reason: msg.reason });
                window.removeEventListener('message', this.handleMessage);
                break;
        }
    };
}
