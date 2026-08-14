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
import { customConsole as console } from '../utils';
import { getWindowMessageRouter, recordWindowMessageDebug } from '../WindowMessageRouter';

type ReservedEventHandler = 'request';
type EventHanlder = string;
interface SendParams<T extends EventHanlder> {
    [x: string]: unknown;
    to?: string;
    module?: string;
    event: T extends ReservedEventHandler ? never : T;
}
type ListenerCallback = (res: MessageEvent) => void;
export interface ListenerHandler { event: EventHanlder; sequence: number }
type TargetWindow = Window;
type GetTragetWindows = () => TargetWindow[];
type SendTargetKey = number | string;
const isConnectorMessage = (data: unknown): boolean => typeof data === 'object' && data !== null && !('channel' in data);
const connectorWindows = new WeakSet<Window>();

const targetWindowName = (target: Window): string => {
    if (typeof document !== 'object') return 'iframe';
    for (const frame of Array.from(document.querySelectorAll('iframe'))) {
        if (frame.contentWindow === target) return frame.name || frame.id || 'iframe';
    }
    return target === window.parent ? 'parent' : 'iframe';
};

abstract class BaseConnector {
    protected readonly _errMsgType = 'postMessage';
    protected invalidFunc: null | (() => void) = null;
    protected _listeners: Map<EventHanlder, Array<ListenerCallback | null>> = new Map();
    private readonly unsubscribeMessage: () => void;

    protected constructor() {
        if (typeof window !== 'object') {
            const errMsg = 'cannot find global Window object, please check your runtime environment';
            console.error(this.printErrMsg(errMsg));
            this.invalidFunc = (): void => { throw new Error(this.printErrMsg(errMsg)); };
        }

        if (connectorWindows.has(window)) {
            throw new Error(this.printErrMsg('only one Connector can be created for each Window'));
        }
        connectorWindows.add(window);
        // Connector 保留原有 event/request 协议，但原生 message 入口由共享 Router 统一管理。
        this.unsubscribeMessage = getWindowMessageRouter().subscribe(this.handleMessage, event => isConnectorMessage(event.data));
    }

    private readonly handleMessage = (event: MessageEvent): void => {
        const data = event.data;
        try {
            if (data?.source === 'react-devtools-content-script') {
                return; // 调试模式不处理 react-devtools 发送的消息
            }
            const listener = this._listeners.get(data?.event);
            if (data?.event === 'request') {
                this.awaitFetch(event);
            } else if (listener) {
                listener.forEach(cb => cb?.(event));
            } else if (['mouseover'].includes(data?.event)) {
                // 鼠标事件，无操作
            } else {
                console.warn(this.printErrMsg('missed [event] in your message, please check your params, or maybe have an invalid send'));
            }
        } catch (e) {
            console.error(e);
        }
    };

    dispose(): void {
        this.unsubscribeMessage();
        this._listeners.clear();
        connectorWindows.delete(window);
    }

    send<T extends EventHanlder>(body: SendParams<T>, reject?: (err: Error) => void): void {
        if (this.invalidFunc) {
            this.invalidFunc();
            return;
        }
        const targetWindows = body.to !== undefined ? this.getClientWindows(body.to) : this.getTargetWindows();
        const isPostMessageInvalid = targetWindows.length === 0 || (body.to !== undefined && targetWindows[0]?.postMessage === undefined);
        if (isPostMessageInvalid) {
            const errMsg = 'missed postMessage function, please check your iframe element';
            console.warn(this.printErrMsg(errMsg));
            reject?.(new Error(errMsg));
            return;
        }
        body.from = window.name;
        const postBody = JSON.stringify(body);
        targetWindows.forEach(targetWindow => {
            recordWindowMessageDebug({
                direction: 'outbound',
                data: body,
                origin: window.location.origin,
                target: body.to || targetWindowName(targetWindow),
            });
            targetWindow.postMessage(postBody, this.getTargetOrigin());
        });
    }

    addListener<T extends EventHanlder>(event: T extends ReservedEventHandler ? never : T, callback: ListenerCallback): ListenerHandler {
        if (this.invalidFunc) {
            this.invalidFunc();
        }
        if (this._listeners.has(event)) {
            (this._listeners.get(event) as ListenerCallback[]).push(callback);
        } else {
            this._listeners.set(event, [callback]);
        }
        return { event, sequence: (this._listeners.get(event) as ListenerCallback[]).length - 1 };
    }

    removeListener({ event, sequence }: ListenerHandler): void {
        if (this.invalidFunc) {
            this.invalidFunc();
        }
        const listeners = this._listeners.get(event);
        if (listeners) {
            listeners[sequence] = null;
            if (listeners.filter(item => item).length === 0) {
                this._listeners.delete(event);
            }
        }
    }

    getTargetOrigin(): string {
        return process.env.NODE_ENV === 'development' || window.location.origin.startsWith('file') ? '*' : window.location.origin;
    }

    protected printErrMsg(errMsg: string): string {
        return `[${this._errMsgType}]: ${errMsg}`;
    }

    protected getClientWindows(windowName?: string): TargetWindow[] {
        const frames = this.getFrameWindow().frames;
        if (windowName) {
            return [(frames as any)[windowName]];
        } else {
            const res: TargetWindow[] = [];
            for (let i = 0; i < frames.length; i++) {
                res.push(frames[i]);
            }
            return res;
        }
    }

    protected abstract getFrameWindow(): TargetWindow;
    protected abstract getTargetWindows(): TargetWindow[];
    protected abstract awaitFetch(e: MessageEvent): void;
}

type FetchSequenceID = number;

interface FetchParams {
    [x: string]: unknown;
    event?: never;
}
interface FetchRequest {
    [x: string]: any;
    event: 'request';
    from: SendTargetKey;
    id: FetchSequenceID;
}
export class ClientConnector extends BaseConnector {
    protected readonly _level: string = 'client';
    private readonly _msgSequence: Map<FetchSequenceID, { resolve: (value: unknown) => void; reject: (value: unknown) => void }> = new Map();
    private _curFetchSequenceID: FetchSequenceID = 0;
    private readonly _module: string;

    constructor({
        module,
    }: {
        getTargetWindow?: GetTragetWindows;
        module: string;
    }) {
        super();
        this._module = module;
        this._level = 'client';
    }

    send<T extends EventHanlder>(body: SendParams<T>, reject?: (err: Error) => void): void {
        body.module = body.module ?? this._module;
        super.send(body, reject);
    }

    async fetch(params: FetchParams): Promise<unknown> {
        return new Promise((resolve, reject) => {
            params.id = this._curFetchSequenceID;
            (params as Record<string, unknown>).event = 'request';
            const body = params as unknown as SendParams<string>;
            this.send(body, reject);
            this._msgSequence.set(this._curFetchSequenceID++, { resolve, reject });
        });
    };

    protected awaitFetch(res: MessageEvent<FetchRequest>): void {
        const result = this._msgSequence.get(res.data.id);
        if (result !== undefined) {
            const { resolve, reject } = result;
            if (res.data.body?.error !== null && res.data.body?.error !== undefined) {
                reject(res.data.body.error);
            } else {
                resolve(res.data);
            }
            this._msgSequence.delete(res.data.id);
        }
    }

    protected getFrameWindow(): TargetWindow {
        return window.parent;
    }

    protected getTargetWindows(): TargetWindow[] {
        return [this.getFrameWindow()];
    }
}

type Response = (res: MessageEvent) => Promise<Record<string, unknown>>;
export class ServerConnector extends BaseConnector {
    protected readonly _getInterceptorHandlers: (command: string) => <T>(event: MessageEvent, responseInterceptor: T) => void;
    protected readonly _sendBefore?: <T extends EventHanlder>(body: SendParams<T>) => SendParams<T>;
    protected readonly _level: string = 'server';
    private _responseForFetch: Response | null = null;

    constructor({ getInterceptorHandlers, sendBefore }: {
        getTargetWindow?: GetTragetWindows;
        getInterceptorHandlers: (command: string) => <T>(event: MessageEvent, responseInterceptor: T) => void;
        sendBefore?: <T extends EventHanlder>(body: SendParams<T>) => SendParams<T>;
    }) {
        super();
        this._getInterceptorHandlers = getInterceptorHandlers;
        this._sendBefore = sendBefore;
    }

    registerAwaitFetch(callback: Response): void {
        this._responseForFetch = callback;
    }

    send<T extends EventHanlder>(body: SendParams<T>, reject?: (err: Error) => void): void {
        let sendbody = body;
        if (this._sendBefore) {
            sendbody = this._sendBefore(sendbody);
        }
        super.send(sendbody, reject);
    }

    protected async awaitFetch(event: MessageEvent): Promise<void> {
        if (typeof event.data.id !== 'number' || !this._responseForFetch) {
            const errMsg = 'something wrong with request response for fetch listener or data.id, please check your config';
            console.error(this.printErrMsg(errMsg));
            return;
        }
        const res = await this._responseForFetch(event);
        // 判断是否对该命令的返回内容进行了拦截配置，如果有配置，则先执行拦截方法逻辑
        const callback = this._getInterceptorHandlers(event.data.args.command);
        if (typeof callback === 'function') {
            callback(event, res);
        }
        this.send({
            event: 'request',
            to: event.data.from,
            id: event.data.id,
            ...res,
        } as SendParams<EventHanlder>);
    }

    protected getFrameWindow(): TargetWindow {
        return window;
    }

    protected getTargetWindows(): TargetWindow[] {
        return this.getClientWindows();
    }
}
