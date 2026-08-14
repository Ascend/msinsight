/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    COMMAND_ERROR_CODES,
    CommandError,
    isCommandErrorShape,
    MODULE_AGENT_CANCEL_COMMAND,
    MODULE_AGENT_COMMAND_ERROR,
    MODULE_AGENT_COMMAND_RESPONSE,
    MODULE_AGENT_COMMANDS_CHANGED,
    MODULE_AGENT_EXECUTE_COMMAND,
    MODULE_AGENT_HELLO,
    MODULE_AGENT_MESSAGE_CHANNEL,
    MODULE_AGENT_OBSERVE,
    MODULE_AGENT_READY,
    type CommandDefinition,
    type JsonObject,
    type ModuleAgentClientMessage,
    type ObservationData,
} from '@insight/lib/FrontendAgentCommand';
import {
    frameWindowMessageOrigin,
    getWindowMessageRouter,
    recordWindowMessageDebug,
    isWindowMessageChannel,
    matchesWindowMessageOrigin,
    withWindowMessageChannel,
} from '@insight/lib/WindowMessageRouter';

interface PendingRequest {
    resolve: (value: unknown) => void;
    reject: (reason: unknown) => void;
    timeoutId: number;
}

interface ModuleFrameTransportOptions {
    moduleId: string;
    frame: HTMLIFrameElement;
    onCommandsChanged: (commands: CommandDefinition[]) => void;
    onDisconnect: () => void;
}

export class ModuleFrameTransport {
    private readonly pending = new Map<string, PendingRequest>();
    private readonly unsubscribe: () => void;
    private readonly onLoad = (): void => this.connect();
    private connectionToken = '';
    private ready = false;
    private disposed = false;

    constructor(private readonly options: ModuleFrameTransportOptions) {
        this.unsubscribe = getWindowMessageRouter().subscribe(
            this.handleMessage as (event: MessageEvent) => void,
            isWindowMessageChannel(MODULE_AGENT_MESSAGE_CHANNEL),
        );
        options.frame.addEventListener('load', this.onLoad);
        this.connect();
    }

    observe(requestId: string, deadline: number): Promise<ObservationData> {
        return this.request<ObservationData>({
            event: MODULE_AGENT_OBSERVE,
            requestId,
            deadline,
        }, requestId, deadline);
    }

    execute(command: string, args: JsonObject, requestId: string, deadline: number): Promise<unknown> {
        return this.request({
            event: MODULE_AGENT_EXECUTE_COMMAND,
            requestId,
            command,
            args,
            deadline,
        }, requestId, deadline);
    }

    async cancel(targetRequestId: string): Promise<void> {
        if (!this.ready || this.disposed) return;
        const requestId = crypto.randomUUID();
        await this.request({
            event: MODULE_AGENT_CANCEL_COMMAND,
            requestId,
            targetRequestId,
        }, requestId, Date.now() + 1000);
    }

    dispose(): void {
        if (this.disposed) return;
        this.disposed = true;
        this.options.frame.removeEventListener('load', this.onLoad);
        this.unsubscribe();
        this.disconnect(connectionLost(`Module '${this.options.moduleId}' iframe was detached.`));
    }

    private connect(): void {
        if (this.disposed) return;
        this.connectionToken = crypto.randomUUID();
        this.ready = false;
        this.disconnect(connectionLost(`Module '${this.options.moduleId}' iframe was reloaded.`));
        this.post({
            event: MODULE_AGENT_HELLO,
            moduleId: this.options.moduleId,
            connectionToken: this.connectionToken,
        });
    }

    private disconnect(error: CommandError): void {
        this.options.onDisconnect();
        [...this.pending.entries()].forEach(([requestId, pending]) => {
            this.finish(requestId);
            pending.reject(error);
        });
    }

    private readonly handleMessage = (event: MessageEvent<ModuleAgentClientMessage>): void => {
        const message = event.data;
        if (!message || message.moduleId !== this.options.moduleId || message.connectionToken !== this.connectionToken) return;
        if (event.source !== this.options.frame.contentWindow || !matchesWindowMessageOrigin(event.origin, this.targetOrigin())) return;
        if (message.event === MODULE_AGENT_READY) {
            this.ready = true;
            return;
        }
        if (message.event === MODULE_AGENT_COMMANDS_CHANGED) {
            if (!this.ready) return;
            try {
                if (!Array.isArray(message.commands)) throw invalid('Module command snapshot must be an array.');
                this.options.onCommandsChanged(message.commands);
            } catch (error) {
                this.ready = false;
                this.disconnect(error instanceof CommandError ? error : invalid(errorMessage(error)));
            }
            return;
        }
        if (message.event !== MODULE_AGENT_COMMAND_RESPONSE && message.event !== MODULE_AGENT_COMMAND_ERROR) return;
        if (typeof message.requestId !== 'string' || !message.requestId) return;
        const pending = this.pending.get(message.requestId);
        if (!pending) return;
        this.finish(message.requestId);
        if (message.event === MODULE_AGENT_COMMAND_RESPONSE) {
            pending.resolve(message.result);
            return;
        }
        pending.reject(isCommandErrorShape(message.error)
            ? new CommandError(message.error)
            : invalid('Module command error response is malformed.'));
    };

    private request<T>(body: object, requestId: string, deadline: number): Promise<T> {
        if (this.disposed) return Promise.reject(connectionLost(`Module '${this.options.moduleId}' iframe was detached.`));
        if (!this.ready) return Promise.reject(unavailable(this.options.moduleId));
        if (Date.now() >= deadline) return Promise.reject(timeout());
        if (this.pending.has(requestId)) return Promise.reject(invalid(`Request '${requestId}' is already pending.`));
        return new Promise<T>((resolve, reject) => {
            const timeoutId = window.setTimeout(() => {
                this.pending.delete(requestId);
                reject(timeout());
            }, Math.max(0, deadline - Date.now()));
            this.pending.set(requestId, { resolve: resolve as (value: unknown) => void, reject, timeoutId });
            this.post({
                ...body,
                moduleId: this.options.moduleId,
                connectionToken: this.connectionToken,
            });
        });
    }

    private post(message: object): void {
        const target = this.options.frame.contentWindow;
        if (!target) return;
        const channelMessage = withWindowMessageChannel(MODULE_AGENT_MESSAGE_CHANNEL, message);
        recordWindowMessageDebug({
            direction: 'outbound',
            data: channelMessage,
            origin: window.location.origin,
            target: this.options.moduleId,
        });
        target.postMessage(channelMessage, this.targetOrigin());
    }

    private finish(requestId: string): void {
        const pending = this.pending.get(requestId);
        if (!pending) return;
        window.clearTimeout(pending.timeoutId);
        this.pending.delete(requestId);
    }

    private targetOrigin(): string {
        return frameWindowMessageOrigin(this.options.frame.src);
    }
}

const errorMessage = (error: unknown): string => error instanceof Error ? error.message : String(error);
const invalid = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.INVALID,
    message,
    retryable: false,
});
const unavailable = (moduleId: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.UNAVAILABLE,
    message: `Module '${moduleId}' command provider is unavailable.`,
    retryable: true,
});
const timeout = (): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.TIMEOUT,
    message: 'The module command exceeded its deadline.',
    retryable: true,
});
const connectionLost = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.CONNECTION_LOST,
    message,
    retryable: true,
});
