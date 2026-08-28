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
    FRONTEND_AGENT_EXECUTE_COMMAND,
    type CommandContext,
    type CommandDefinition,
    type CommandHandler,
    type FrontendAgentExecuteCommandMessage,
    type JsonObject,
    type ObservationData,
    type ObservationProvider,
    withAbortSignal,
} from '@insight/lib/FrontendAgentCommand';
import { AgentFrameTransport } from './AgentFrameTransport';
import { CommandCatalog, type GlobalCommandEntry } from './CommandCatalog';
import { ModuleFrameTransport } from './ModuleFrameTransport';

interface RunningCommand {
    controller: AbortController;
    cancel?: () => Promise<void> | void;
}

const DEBUG_COMMAND_TIMEOUT_MS = 30000;

export class FrontendAgentCommandController {
    private readonly catalog = new CommandCatalog();
    private readonly moduleTransports = new Map<string, ModuleFrameTransport>();
    private readonly running = new Map<string, RunningCommand>();
    private agentTransport?: AgentFrameTransport;
    private activeModule = '';
    private frameworkObservation?: ObservationProvider;
    private disposed = false;

    registerGlobalCommand(definition: CommandDefinition, handler: CommandHandler): () => void {
        this.ensureActive();
        return this.catalog.registerGlobal(definition, handler);
    }

    setFrameworkObservationProvider(provider: ObservationProvider): () => void {
        this.ensureActive();
        this.frameworkObservation = provider;
        return () => {
            if (this.frameworkObservation === provider) this.frameworkObservation = undefined;
        };
    }

    attachAgentFrame(frame: HTMLIFrameElement): () => void {
        this.ensureActive();
        if (this.agentTransport) {
            this.agentTransport.dispose();
            this.abortRunning(connectionLost('The Agent iframe connection was replaced.'));
        }
        const transport = new AgentFrameTransport({
            frame,
            execute: message => this.execute(message),
            cancel: targetRequestId => this.cancel(targetRequestId),
            onReload: () => this.abortRunning(connectionLost('The Agent iframe was reloaded.')),
        });
        this.agentTransport = transport;
        return () => {
            if (this.agentTransport !== transport) return;
            transport.dispose();
            this.agentTransport = undefined;
            this.abortRunning(connectionLost('The Agent iframe was detached.'));
        };
    }

    attachModuleFrame(moduleId: string, frame: HTMLIFrameElement): () => void {
        this.ensureActive();
        this.detachModule(moduleId);
        const transport = new ModuleFrameTransport({
            moduleId,
            frame,
            onCommandsChanged: commands => this.catalog.replaceModule(moduleId, commands),
            onDisconnect: () => this.catalog.removeModule(moduleId),
        });
        this.moduleTransports.set(moduleId, transport);
        return () => {
            if (this.moduleTransports.get(moduleId) !== transport) return;
            this.detachModule(moduleId);
        };
    }

    setActiveModule(moduleId: string): void {
        this.ensureActive();
        this.activeModule = moduleId;
    }

    /**
     * 重放一条已捕获的前端命令，走与 Agent 请求完全相同的执行路径
     * （目录查找、help/observe 内置命令处理、模块 transport），仅跳过 agent iframe 这一跳。
     * 仅供 window message 调试面板使用，用于定位命令失败的具体环节。
     */
    replayCommandForDebug(command: string, args: JsonObject): Promise<unknown> {
        this.ensureActive();
        return this.execute({
            event: FRONTEND_AGENT_EXECUTE_COMMAND,
            requestId: crypto.randomUUID(),
            command,
            args,
            deadline: Date.now() + DEBUG_COMMAND_TIMEOUT_MS,
        });
    }

    dispose(): void {
        if (this.disposed) return;
        this.disposed = true;
        this.agentTransport?.dispose();
        this.agentTransport = undefined;
        this.abortRunning(connectionLost('The framework command controller was disposed.'));
        [...this.moduleTransports.keys()].forEach(moduleId => this.detachModule(moduleId));
        this.frameworkObservation = undefined;
    }

    private async execute(message: FrontendAgentExecuteCommandMessage): Promise<unknown> {
        this.ensureActive();
        if (typeof message.command !== 'string' || !message.command.trim() ||
            typeof message.requestId !== 'string' || !message.requestId.trim() ||
            !Number.isFinite(message.deadline)) {
            throw invalid('Command request is malformed.');
        }
        const commandName = message.command;
        const args = asJsonObject(message.args);
        if (Date.now() >= message.deadline) throw timeout();
        if (this.running.has(message.requestId)) throw invalid(`Request '${message.requestId}' is already running.`);
        const entry = this.catalog.getVisible(commandName, this.activeModule);
        if (!entry) throw unavailable(commandName);
        if (commandName === 'help') return this.help(args);
        const controller = new AbortController();
        const timeoutId = window.setTimeout(() => {
            controller.abort(timeout());
            void ignoreCancelError(this.running.get(message.requestId)?.cancel);
        }, message.deadline - Date.now());
        try {
            if (commandName === 'observe') {
                validateArgs(args, []);
                const moduleTransport = this.activeModule ? this.moduleTransports.get(this.activeModule) : undefined;
                this.running.set(message.requestId, {
                    controller,
                    cancel: moduleTransport ? () => moduleTransport.cancel(message.requestId) : undefined,
                });
                return await this.observe(moduleTransport, message.requestId, message.deadline, controller.signal);
            }
            if (entry.source === 'global') {
                this.running.set(message.requestId, { controller });
                return await this.executeGlobal(entry, args, message, controller.signal);
            }
            if (entry.source !== 'module') throw unavailable(commandName);
            const transport = this.requireModuleTransport(entry.moduleId);
            this.running.set(message.requestId, {
                controller,
                cancel: () => transport.cancel(message.requestId),
            });
            return await withAbortSignal(
                transport.execute(entry.definition.name, args, message.requestId, message.deadline),
                controller.signal,
                cancelled,
            );
        } finally {
            window.clearTimeout(timeoutId);
            if (this.running.get(message.requestId)?.controller === controller) this.running.delete(message.requestId);
        }
    }

    private help(args: JsonObject): JsonObject {
        validateArgs(args, ['command']);
        const commandName = args.command;
        if (commandName !== undefined && (typeof commandName !== 'string' || !commandName.trim())) {
            throw invalid('help.command must be a non-empty string.');
        }
        if (typeof commandName === 'string') {
            const entry = this.catalog.getVisible(commandName, this.activeModule);
            if (!entry) throw unavailableHelpCommand(commandName);
            return { command: toJsonObject(entry.definition) };
        }
        return {
            commands: this.catalog.listVisible(this.activeModule).map(({ name, title, description, approval }) => {
                const summary: JsonObject = { name, title, description };
                if (approval) summary.approval = approval;
                return summary;
            }),
        };
    }

    private async observe(
        moduleTransport: ModuleFrameTransport | undefined,
        requestId: string,
        deadline: number,
        signal: AbortSignal,
    ): Promise<ObservationData> {
        const appPromise = this.frameworkObservation ? this.frameworkObservation(signal) : {};
        const modulePromise = this.observeModule(moduleTransport, requestId, deadline);
        try {
            const [app, module] = await withAbortSignal(Promise.all([appPromise, modulePromise]), signal, cancelled);
            return { collectedAt: Date.now(), app, module };
        } catch (error) {
            if (moduleTransport && !signal.aborted) await ignoreCancelError(() => moduleTransport.cancel(requestId));
            throw error;
        }
    }

    private async observeModule(
        moduleTransport: ModuleFrameTransport | undefined,
        requestId: string,
        deadline: number,
    ): Promise<ObservationData> {
        if (!moduleTransport) return {};
        try {
            return await moduleTransport.observe(requestId, deadline);
        } catch (error) {
            const shape = error instanceof CommandError ? error.toJSON() : connectionLost(String(error)).toJSON();
            return { error: toJsonObject(shape) };
        }
    }

    private async executeGlobal(
        entry: GlobalCommandEntry,
        args: JsonObject,
        message: FrontendAgentExecuteCommandMessage,
        signal: AbortSignal,
    ): Promise<unknown> {
        const context: CommandContext = {
            requestId: message.requestId,
            deadline: message.deadline,
            signal,
        };
        return withAbortSignal(entry.handler(args, context), signal, cancelled);
    }

    private async cancel(targetRequestId: string): Promise<void> {
        const running = this.running.get(targetRequestId);
        if (!running) return;
        running.controller.abort(cancelled());
        await ignoreCancelError(running.cancel);
        this.running.delete(targetRequestId);
    }

    private detachModule(moduleId: string): void {
        this.moduleTransports.get(moduleId)?.dispose();
        this.moduleTransports.delete(moduleId);
        this.catalog.removeModule(moduleId);
    }

    private abortRunning(error: CommandError): void {
        this.running.forEach(({ controller, cancel }) => {
            controller.abort(error);
            void ignoreCancelError(cancel);
        });
        this.running.clear();
    }

    private requireModuleTransport(moduleId: string): ModuleFrameTransport {
        const transport = this.moduleTransports.get(moduleId);
        if (transport) return transport;
        throw unavailable(moduleId);
    }

    private ensureActive(): void {
        if (this.disposed) throw connectionLost('The framework command controller has been disposed.');
    }
}

const asJsonObject = (value: unknown): JsonObject => {
    if (value === undefined) return {};
    if (!value || typeof value !== 'object' || Array.isArray(value)) throw invalid('Command args must be an object.');
    return value as JsonObject;
};
const toJsonObject = (value: object): JsonObject => value as JsonObject;
const validateArgs = (args: JsonObject, allowed: string[]): void => {
    const unknown = Object.keys(args).find(key => !allowed.includes(key));
    if (unknown) throw invalid(`Unknown command argument '${unknown}'.`);
};
const ignoreCancelError = async (cancel?: () => Promise<void> | void): Promise<void> => {
    await Promise.resolve().then(cancel).catch(() => undefined);
};
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
const unavailableHelpCommand = (name: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.UNAVAILABLE,
    message: `Command '${name}' is unavailable. Use help with empty args to discover the current fully qualified command names.`,
    retryable: false,
});
const timeout = (): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.TIMEOUT,
    message: 'The frontend command exceeded its deadline.',
    retryable: true,
});
const cancelled = (): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.CANCELLED,
    message: 'The frontend command was cancelled.',
    retryable: true,
});
const connectionLost = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.CONNECTION_LOST,
    message,
    retryable: true,
});
