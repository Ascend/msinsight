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
import { customConsole as console } from '../utils/Console';
import { RequestChannel, type FetchParams } from './RequestChannel';
export { RequestChannel } from './RequestChannel';

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
abstract class BaseConnector {
    protected readonly _errMsgType = 'postMessage';
    protected invalidFunc: null | (() => void) = null;
    protected _listeners: Map<EventHanlder, Array<ListenerCallback | null>> = new Map();

    protected constructor() {
        if (typeof window !== 'object') {
            const errMsg = 'cannot find global Window object, please check your runtime environment';
            console.error(this.printErrMsg(errMsg));
            this.invalidFunc = (): void => { throw new Error(this.printErrMsg(errMsg)); };
        }

        window.onmessage = (event: MessageEvent): void => {
            const res = { ...event };
            try {
                if (typeof event.data === 'string') {
                    res.data = JSON.parse(event.data);
                } else {
                    res.data = event.data;
                }
                if (res.data?.source === 'react-devtools-content-script') {
                    return; // 调试模式不处理 react-devtools 发送的消息
                }
                const listener = this._listeners.get(res.data.event);
                if (res.data.event === 'request') {
                    this.awaitFetch(res);
                } else if (listener) {
                    listener.forEach(cb => cb?.(res));
                } else if (['mouseover'].includes(res.data.event)) {
                    // 鼠标事件，无操作
                } else {
                    console.warn(this.printErrMsg('missed [event] in your message, please check your params, or maybe have an invalid send'));
                }
            } catch (e) {
                console.error(e);
            }
        };
        Object.defineProperty(window, 'onmessage', {
            configurable: false,
            set: () => {
                console.warn(this.printErrMsg('the property \'window.onmessage\' in used, reassign it is invalid'));
            },
        });
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

export class ClientConnector extends BaseConnector {
    protected readonly _level: string = 'client';
    private readonly requests = new RequestChannel((message, onError) => this.send<string>(message, onError));
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

    fetch(params: FetchParams, signal?: AbortSignal): Promise<unknown> {
        return this.requests.fetch(params, signal);
    }

    protected awaitFetch(res: MessageEvent): void {
        this.requests.receive(res.data);
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
    private readonly frameInstances = new WeakMap<Window, symbol>();

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

    // A slow backend operation must not keep a removed iframe alive while waiting for its reply.
    private frameInstance(name: string): symbol | undefined {
        if (!name) { return undefined; }
        const [frame] = this.getClientWindows(name);
        if (!frame) { return undefined; }
        let instance = this.frameInstances.get(frame);
        if (!instance) {
            instance = Symbol(name);
            this.frameInstances.set(frame, instance);
        }
        return instance;
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
        const instance = this.frameInstance(event.data.from);
        const res = await this._responseForFetch(event);
        if (instance !== this.frameInstance(event.data.from)) { return; }
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
