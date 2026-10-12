/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    ACP_MESSAGE_CHANNEL,
    FRONTEND_AGENT_CANCEL_COMMAND,
    FRONTEND_AGENT_COMMAND_ERROR,
    FRONTEND_AGENT_COMMAND_RESPONSE,
    FRONTEND_AGENT_EXECUTE_COMMAND,
    toCommandError,
    type FrontendAgentCommandRequestMessage,
} from '@insight/lib/FrontendAgentCommand';
import {
    frameWindowMessageOrigin,
    getWindowMessageRouter,
    recordWindowMessageDebug,
    isWindowMessageChannel,
    matchesWindowMessageOrigin,
    normalizeWindowMessageOrigin,
    withWindowMessageChannel,
} from '@insight/lib/WindowMessageRouter';

interface AgentFrameTransportOptions {
    frame: HTMLIFrameElement;
    execute: (message: Extract<FrontendAgentCommandRequestMessage, { event: typeof FRONTEND_AGENT_EXECUTE_COMMAND }>) => Promise<unknown>;
    cancel: (targetRequestId: string) => Promise<void> | void;
    onReload: () => void;
}

export class AgentFrameTransport {
    private readonly unsubscribe: () => void;
    private readonly onLoad = (): void => this.options.onReload();
    private disposed = false;

    constructor(private readonly options: AgentFrameTransportOptions) {
        this.unsubscribe = getWindowMessageRouter().subscribe(
            this.handleMessage as (event: MessageEvent) => void,
            isWindowMessageChannel(ACP_MESSAGE_CHANNEL),
        );
        options.frame.addEventListener('load', this.onLoad);
    }

    dispose(): void {
        if (this.disposed) return;
        this.disposed = true;
        this.options.frame.removeEventListener('load', this.onLoad);
        this.unsubscribe();
    }

    private readonly handleMessage = (event: MessageEvent<FrontendAgentCommandRequestMessage>): void => {
        if (event.source !== this.options.frame.contentWindow || !matchesWindowMessageOrigin(event.origin, this.targetOrigin())) return;
        const message = event.data;
        if (typeof message?.requestId !== 'string' || !message.requestId) return;
        if (message.event === FRONTEND_AGENT_CANCEL_COMMAND) {
            if (typeof message.targetRequestId !== 'string' || !message.targetRequestId) return;
            void Promise.resolve(this.options.cancel(message.targetRequestId)).then(
                () => this.respond(event.origin, message.requestId, undefined),
                error => this.reject(event.origin, message.requestId, error),
            );
            return;
        }
        if (message.event !== FRONTEND_AGENT_EXECUTE_COMMAND) return;
        void this.options.execute(message).then(
            result => this.respond(event.origin, message.requestId, result),
            error => this.reject(event.origin, message.requestId, error),
        );
    };

    private respond(origin: string, requestId: string, result: unknown): void {
        this.post(origin, {
            event: FRONTEND_AGENT_COMMAND_RESPONSE,
            requestId,
            result,
        });
    }

    private reject(origin: string, requestId: string, error: unknown): void {
        this.post(origin, {
            event: FRONTEND_AGENT_COMMAND_ERROR,
            requestId,
            error: toCommandError(error),
        });
    }

    private post(origin: string, message: object): void {
        if (this.disposed) return;
        const target = this.options.frame.contentWindow;
        if (!target) return;
        const channelMessage = withWindowMessageChannel(ACP_MESSAGE_CHANNEL, message);
        recordWindowMessageDebug({
            direction: 'outbound',
            data: channelMessage,
            origin: window.location.origin,
            target: this.options.frame.name || this.options.frame.id || 'AcpSession',
        });
        target.postMessage(channelMessage, normalizeWindowMessageOrigin(origin));
    }

    private targetOrigin(): string {
        return frameWindowMessageOrigin(this.options.frame.src);
    }
}
