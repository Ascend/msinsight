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
    MODULE_AGENT_CANCEL_COMMAND,
    MODULE_AGENT_COMMAND_ERROR,
    MODULE_AGENT_COMMAND_RESPONSE,
    MODULE_AGENT_COMMANDS_CHANGED,
    MODULE_AGENT_EXECUTE_COMMAND,
    MODULE_AGENT_HELLO,
    MODULE_AGENT_MESSAGE_CHANNEL,
    MODULE_AGENT_OBSERVE,
    MODULE_AGENT_READY,
    toCommandError,
    validateCommandDefinition,
    withAbortSignal,
    type CommandDefinition,
    type CommandHandler,
    type JsonObject,
    type ModuleAgentCancelCommandMessage,
    type ModuleAgentControllerMessage,
    type ModuleAgentExecuteCommandMessage,
    type ModuleAgentHelloMessage,
    type ModuleAgentObserveMessage,
    type ObservationProvider,
} from '../FrontendAgentCommand';
import {
    getWindowMessageRouter,
    isWindowMessageChannel,
    recordWindowMessageDebug,
    matchesWindowMessageOrigin,
    normalizeWindowMessageOrigin,
    parentWindowMessageOrigin,
    withWindowMessageChannel,
} from '../WindowMessageRouter';

export interface ModuleAgentCommandClientOptions {
    moduleId: string;
    observe: ObservationProvider;
    parentOrigin?: string;
}

interface RegisteredCommand {
    definition: CommandDefinition;
    handler: CommandHandler;
}

interface RunningCommand {
    controller: AbortController;
    connectionToken: string;
}

export class ModuleAgentCommandClient {
    private readonly commands = new Map<string, RegisteredCommand>();
    private readonly running = new Map<string, RunningCommand>();
    private unsubscribe?: () => void;
    private connectionToken?: string;
    private responseOrigin?: string;
    private snapshotScheduled = false;
    private started = false;

    constructor(private readonly options: ModuleAgentCommandClientOptions) {
        if (!options.moduleId) throw invalid('moduleId is required.');
    }

    registerCommand(definition: CommandDefinition, handler: CommandHandler): () => void {
        validateCommandDefinition(definition);
        this.validateCommandName(definition.name);
        if (this.commands.has(definition.name)) throw invalid(`Command '${definition.name}' is already registered.`);
        const command = { definition, handler };
        this.commands.set(definition.name, command);
        this.sendCommandsChanged();
        return () => {
            if (this.commands.get(definition.name) !== command) return;
            this.commands.delete(definition.name);
            this.sendCommandsChanged();
        };
    }

    start(): () => void {
        if (this.started) return () => this.dispose();
        this.started = true;
        this.unsubscribe = getWindowMessageRouter().subscribe(
            this.handleMessage as (event: MessageEvent) => void,
            isWindowMessageChannel(MODULE_AGENT_MESSAGE_CHANNEL),
        );
        return () => this.dispose();
    }

    dispose(): void {
        if (!this.started) return;
        this.started = false;
        this.unsubscribe?.();
        this.unsubscribe = undefined;
        this.connectionToken = undefined;
        this.responseOrigin = undefined;
        this.abortRunning(connectionLost());
    }

    private readonly handleMessage = (event: MessageEvent<ModuleAgentControllerMessage>): void => {
        const message = event.data;
        if (!message || message.moduleId !== this.options.moduleId) return;
        if (event.source !== window.parent || !this.matchesParentOrigin(event.origin)) return;
        if (message.event === MODULE_AGENT_HELLO) {
            this.connect(message, event.origin);
            return;
        }
        if (!this.connectionToken || message.connectionToken !== this.connectionToken) return;
        if (message.event === MODULE_AGENT_CANCEL_COMMAND) {
            this.cancel(message);
            return;
        }
        if (message.event === MODULE_AGENT_OBSERVE || message.event === MODULE_AGENT_EXECUTE_COMMAND) {
            void this.execute(message);
        }
    };

    private connect(message: ModuleAgentHelloMessage, origin: string): void {
        if (typeof message.connectionToken !== 'string' || !message.connectionToken) return;
        if (this.connectionToken !== message.connectionToken) this.abortRunning(connectionLost());
        this.connectionToken = message.connectionToken;
        this.responseOrigin = normalizeWindowMessageOrigin(origin);
        this.post({
            event: MODULE_AGENT_READY,
            moduleId: this.options.moduleId,
            connectionToken: message.connectionToken,
        });
        this.sendCommandsChanged();
    }

    private async execute(message: ModuleAgentObserveMessage | ModuleAgentExecuteCommandMessage): Promise<void> {
        if (typeof message.requestId !== 'string' || !message.requestId.trim() || !Number.isFinite(message.deadline)) return;
        const connectionToken = message.connectionToken;
        if (this.running.has(message.requestId)) {
            this.reject(connectionToken, message.requestId, invalid(`Request '${message.requestId}' is already running.`));
            return;
        }
        if (Date.now() >= message.deadline) {
            this.reject(connectionToken, message.requestId, timeout());
            return;
        }
        const controller = new AbortController();
        const timeoutId = window.setTimeout(() => controller.abort(timeout()), message.deadline - Date.now());
        this.running.set(message.requestId, { controller, connectionToken });
        try {
            const execution = message.event === MODULE_AGENT_OBSERVE
                ? this.options.observe(controller.signal)
                : this.invoke(message, controller.signal);
            this.respond(connectionToken, message.requestId, await withAbortSignal(execution, controller.signal, cancelled));
        } catch (error) {
            this.reject(connectionToken, message.requestId, error);
        } finally {
            window.clearTimeout(timeoutId);
            if (this.running.get(message.requestId)?.controller === controller) this.running.delete(message.requestId);
        }
    }

    private invoke(message: ModuleAgentExecuteCommandMessage, signal: AbortSignal): Promise<unknown> | unknown {
        if (typeof message.command !== 'string' || !message.command.trim()) throw invalid('command is required.');
        if (!message.args || typeof message.args !== 'object' || Array.isArray(message.args)) throw invalid('args must be an object.');
        const command = this.commands.get(message.command);
        if (!command) throw unavailable(message.command);
        return command.handler(message.args, {
            requestId: message.requestId,
            deadline: message.deadline,
            signal,
        });
    }

    private cancel(message: ModuleAgentCancelCommandMessage): void {
        if (typeof message.requestId !== 'string' || !message.requestId.trim() ||
            typeof message.targetRequestId !== 'string' || !message.targetRequestId) return;
        const running = this.running.get(message.targetRequestId);
        if (running?.connectionToken === message.connectionToken) running.controller.abort(cancelled());
        this.respond(message.connectionToken, message.requestId, undefined);
    }

    private sendCommandsChanged(): void {
        if (!this.started || !this.connectionToken || this.snapshotScheduled) return;
        this.snapshotScheduled = true;
        queueMicrotask(() => {
            this.snapshotScheduled = false;
            if (!this.started || !this.connectionToken) return;
            this.post({
                event: MODULE_AGENT_COMMANDS_CHANGED,
                moduleId: this.options.moduleId,
                connectionToken: this.connectionToken,
                commands: [...this.commands.values()].map(({ definition }) => definition),
            });
        });
    }

    private respond(connectionToken: string, requestId: string, result: unknown): void {
        if (!this.started) return;
        this.post({
            event: MODULE_AGENT_COMMAND_RESPONSE,
            moduleId: this.options.moduleId,
            connectionToken,
            requestId,
            result,
        });
    }

    private reject(connectionToken: string, requestId: string, error: unknown): void {
        if (!this.started) return;
        this.post({
            event: MODULE_AGENT_COMMAND_ERROR,
            moduleId: this.options.moduleId,
            connectionToken,
            requestId,
            error: toCommandError(error),
        });
    }

    private post(message: object): void {
        const channelMessage = withWindowMessageChannel(MODULE_AGENT_MESSAGE_CHANNEL, message);
        recordWindowMessageDebug({
            direction: 'outbound',
            data: channelMessage,
            origin: window.location.origin,
            target: 'parent',
        });
        window.parent.postMessage(channelMessage, this.responseOrigin ?? this.expectedParentOrigin());
    }

    private validateCommandName(name: string): void {
        if (!name.startsWith(`${this.options.moduleId}.`)) {
            throw invalid(`Module command '${name}' must use the '${this.options.moduleId}.' namespace.`);
        }
    }

    private expectedParentOrigin(): string {
        return this.options.parentOrigin ?? parentWindowMessageOrigin();
    }

    private matchesParentOrigin(origin: string): boolean {
        return matchesWindowMessageOrigin(origin, this.expectedParentOrigin());
    }

    private abortRunning(error: CommandError): void {
        this.running.forEach(({ controller }) => controller.abort(error));
        this.running.clear();
    }
}

const invalid = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.INVALID,
    message,
    retryable: false,
});
const unavailable = (name: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.UNAVAILABLE,
    message: `Command '${name}' is unavailable.`,
    retryable: false,
});
const timeout = (): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.TIMEOUT,
    message: 'The module command exceeded its deadline.',
    retryable: true,
});
const cancelled = (): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.CANCELLED,
    message: 'The module command was cancelled.',
    retryable: true,
});
const connectionLost = (): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.CONNECTION_LOST,
    message: 'The module command connection was replaced.',
    retryable: true,
});
