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
import type { ConnectHost, DataRequest, ModuleName, Notification, Request, Response, ResponseHandler } from './defs';
import { CONTENT_LENGTH_PREFIX, isResponse, LOCAL_HOST, PORT } from './defs';
import connector from '@/connection';
import { Modal } from 'antd';
import { errorCenter, WsError, ErrorCode } from '@insight/lib/utils';
import { connectRemote } from '../server';
import { store } from '../../store';
import { runInAction } from 'mobx';
import i18n from '@insight/lib/i18n';
import { WebviewSocket } from '@/vscode-adapter/WebviewSocket';
import { isVscodeEnv } from '@/vscode-adapter';

const createRequestHead = function (
    id: number,
    module: string,
    command: string,
    args: Request['params'],
): Request {
    const params = {};
    Object.assign(params, args);
    let fileId = (params as any).dbPath ?? '';
    if (module === 'leaks') {
        fileId = store.sessionStore.activeSession?.activeDataSource?.selectedFilePath ?? '';
    }
    return {
        id,
        moduleName: module,
        type: 'request',
        command,
        fileId,
        projectName: store.sessionStore.activeSession?.activeDataSource?.projectName ?? '',
        params,
    };
};

const MAX_RESPONSE_HANDLERS = 1000;

export interface ErrorMsg {
    error: {
        code: number;
        message: string;
    };
};

export class Connection {
    private readonly _ws: WebSocket | WebviewSocket | undefined;
    private readonly _host: ConnectHost;
    private _msgId: number = 0;
    private readonly _responseHandlers: Map<number, ResponseHandler> = new Map();
    private readonly _requestMsg: Map<number, Request> = new Map();
    private _fetchFlag: boolean = true;

    constructor(initHost: ConnectHost) {
        console.info('[connector]', 'init');
        if (this._ws !== undefined) {
            // wedge: close and release the old websocket
        }
        this._msgId = 0;
        const session = store.sessionStore.activeSession;

        // VSCode 环境使用 WebviewSocket
        if (isVscodeEnv()) {
            const { ws, toIframeUrl } = WebviewSocket.createForHost(initHost);
            this._ws = ws;
            if (toIframeUrl) {
                runInAction(() => {
                    session.toIframeUrl = toIframeUrl;
                });
            }
        } else {
            // 原有逻辑：使用原生 WebSocket
            const protocol = `${window.location.protocol === 'https:' && window.location.host !== 'wry.localhost' ? 'wss:' : 'ws:'}//`;
            if (initHost.jupyterlabProxy) {
                const { host, search } = window.location;
                const path = `${window.location.pathname.replace(/\/resources\/profiler\/frontend/, `/proxy/${initHost.port}/resources/profiler/frontend`)}`;
                const uri = protocol + host + path + search;
                this._ws = new WebSocket(uri);
                runInAction(() => {
                    session.toIframeUrl = `${protocol}${host}${window.location.pathname.replace(/\/resources\/profiler\/frontend\/.*/, `/proxy/${initHost.port}`)}`;
                });
            } else if (!window.location.pathname.includes('/proxy/')) {
                const hostname = window.location.hostname || LOCAL_HOST;
                let pathname = window.location.pathname && window.location.pathname !== '/' ? window.location.pathname.replace(/\/resources\/profiler\/frontend\/index.html/, '') : '';
                pathname = pathname.replace(/\/index.html/, '');
                this._ws = new WebSocket(`${protocol}${hostname}${pathname}:${initHost.port}${window.location.search}`);
                runInAction(() => {
                    session.toIframeUrl = `${protocol}${hostname}${pathname}:${initHost.port}`;
                });
            } else {
                const { location } = window;
                const { host } = location;
                const path = `${window.location.pathname}`.replace(/proxy\/\d{4}/, `proxy/${initHost.port}`);
                const { search } = location;
                const uri = protocol + host + path + search;

                this._ws = new WebSocket(uri);
                runInAction(() => {
                    session.toIframeUrl = `${protocol}${host}${path.replace(/\/index.html/, '')}`;
                });
            }
        }
        this._host = initHost;
    }

    get isConnected(): boolean {
        return this._ws?.readyState === WebSocket.OPEN;
    }

    async reset(): Promise<void> {
        console.info('[connector]', 'reset');
    }

    disconnect(): void {
        if (!this._ws) {
            throw Error('connection is not initialized');
        }
        this._ws.close();
    }

    async connect(): Promise<void> {
        this._fetchFlag = true;
        return new Promise((resolve, reject) => {
            if (!this._ws) {
                reject(new Error('connection is not initialized'));
                return;
            }
            this._ws.onopen = (ev: Event): void => {
                console.info('[connector]', 'onopen');
                // 开始心跳检查
                this.initHeartCheck();
            };

            this._ws.onmessage = (ev: MessageEvent<string>): void => {
                resolve();
            };

            this._ws.onerror = (ev: Event): void => {
                errorCenter.handleError(new WsError(ErrorCode.WS_ERROR, i18n.t('framework:error.ConnectionErrorMessage')));
                console.error('[connector]', ev);
                reject(new Error('connect failed.'));
            };
            this._ws.onclose = (ev: Event): void => {
                Modal.error({
                    title: i18n.t('framework:error.ConnectionClosed'),
                    content: i18n.t('framework:error.ConnectionClosedMessage'),
                    okText: i18n.t('framework:error.Reconnect'),
                    onOk: () => connectRemote(this._host),
                    closable: true,
                });
            };
        }) as Promise<void>;
    }

    async fetch<T>(module: ModuleName, dataRequest: DataRequest): Promise<T | ErrorMsg> {
        if (!this.isConnected) {
            return Promise.reject(new Error(i18n.t('framework:error.ConnectionNotAvailable')));
        }
        if (this._ws === undefined) {
            return Promise.reject(new Error('connection is not initialized'));
        }
        if (this._ws.onmessage !== null && this._fetchFlag) {
            this._fetchFlag = false;
            // message process func
            this._ws.onmessage = this.fetchDataOnMessage;
        }
        return new Promise((resolve: (v: T | ErrorMsg) => void, reject) => {
            const id = this._msgId++;
            const msg: Request = createRequestHead(
                id,
                module,
                dataRequest.command,
                dataRequest.params,
            );
            this.request(msg);
            const reqCallback = this.getCallback(resolve);
            if (this._responseHandlers.size > MAX_RESPONSE_HANDLERS) {
                const firstKey = this._responseHandlers.keys().next().value;
                this._responseHandlers.delete(firstKey);
                this._requestMsg.delete(firstKey);
            }
            this._responseHandlers.set(id, reqCallback);
            this._requestMsg.set(id, msg);
        });
    }

    getCallback<T>(resolve: (p: T | ErrorMsg) => void): (res: Response) => void {
        return (res: Response): void => {
            if (res.command === 'import/action') {
                // 兼容 import/action
                resolve(res as any);
            } else if (res.result && res.body !== undefined) {
                // wedge: return cache resolve
                resolve(res.body as T);
            } else {
                const { code, message } = res.error ?? { code: -1, message: 'Unknown error' };
                resolve({ error: { code, message } } as ErrorMsg);
            }
        };
    }

    fetchDataOnMessage = (ev: MessageEvent<string>): void => {
        if (ev.data.startsWith(CONTENT_LENGTH_PREFIX)) {
            // ignore this message
            return;
        }
        let msg: Response | Notification;
        try {
            msg = JSON.parse(ev.data);
        } catch {
            console.warn('cannot parse json data:', ev);
            return;
        }

        // handle notifications
        if (!isResponse(msg)) {
            if (msg.event === 'parse/leaksMemoryCompleted') {
                const { toBeActivedProject } = store.sessionStore.activeSession;
                if (toBeActivedProject !== undefined && toBeActivedProject.selectedFilePath !== msg.body.dbPath) {
                    console.warn(`event #${msg.event} has been abandoned`);
                    return;
                }
            }
            msg.body.dataSource = this._host;
            connector.send({ ...msg });
            return;
        }

        // handle response
        const reqId = msg.requestId;
        const callback = this._responseHandlers.get(reqId);
        // 抛弃和当前激活项目不一致的请求结果
        const requestMsg = this._requestMsg.get(reqId);
        const currentProjectName = store.sessionStore.activeSession?.activeDataSource?.projectName ?? '';
        const isImport = requestMsg?.command === 'import/action';
        const isOldRequest = requestMsg?.projectName !== undefined && requestMsg.projectName !== currentProjectName;
        if (!isImport && isOldRequest) {
            console.warn(`request #${reqId} has been abandoned`);
            this._requestMsg.delete(reqId);
            this._responseHandlers.delete(reqId);
            return;
        }

        if (callback === undefined) {
            console.warn(`handler for msg #${reqId} not found`);
            return;
        }
        this._requestMsg.delete(reqId);
        this._responseHandlers.delete(reqId);
        callback(msg);
    };

    async findServerPort(): Promise<number> {
        // wedge: implement the true logic
        return Promise.resolve(PORT);
    }

    private async request(msg: Request): Promise<void> {
        const msgStr = JSON.stringify(msg);
        if (this._ws === undefined) {
            throw new Error('');
        }
        this._ws.send(msgStr);
    }

    private initHeartCheck(): void {
        this.sendHeartCheck();
        setTimeout(() => {
            this.initHeartCheck();
        }, 30 * 1000);
    }

    private sendHeartCheck(): void {
        const msg: Request = createRequestHead(this._msgId++, 'global', 'heartCheck', {});
        this.request(msg);
    }
}
