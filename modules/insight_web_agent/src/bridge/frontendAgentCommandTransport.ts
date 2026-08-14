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
    COMMAND_ERROR_CODES,
    CommandError,
    FRONTEND_AGENT_CANCEL_COMMAND,
    FRONTEND_AGENT_COMMAND_ERROR,
    FRONTEND_AGENT_COMMAND_RESPONSE,
    FRONTEND_AGENT_EXECUTE_COMMAND,
    type FrontendAgentCommandResultMessage,
} from '@insight/lib/FrontendAgentCommand';
import {
    getWindowMessageRouter,
    isWindowMessageChannel,
    matchesWindowMessageOrigin,
    parentWindowMessageOrigin,
    withWindowMessageChannel,
} from '@insight/lib/WindowMessageRouter';

interface PendingCommand {
    resolve: (value: unknown) => void;
    reject: (reason: unknown) => void;
    timeoutId: number;
}

const pending = new Map<string, PendingCommand>();
const parentOrigin = parentWindowMessageOrigin();

getWindowMessageRouter().subscribe((event: MessageEvent<FrontendAgentCommandResultMessage>) => {
    if (event.source !== window.parent || !matchesWindowMessageOrigin(event.origin, parentOrigin)) return;
    const message = event.data;
    if (message?.event !== FRONTEND_AGENT_COMMAND_RESPONSE && message?.event !== FRONTEND_AGENT_COMMAND_ERROR) return;
    const command = pending.get(message.requestId);
    if (!command) return;
    finish(message.requestId);
    if (message.event === FRONTEND_AGENT_COMMAND_RESPONSE) command.resolve(message.result);
    else command.reject(new CommandError(message.error));
}, isWindowMessageChannel(ACP_MESSAGE_CHANNEL));

export const executeFrontendCommand = (
    command: string,
    args: Record<string, unknown>,
    requestId: string,
    deadline: number,
): Promise<unknown> => {
    if (pending.has(requestId)) return Promise.reject(new Error(`Command request '${requestId}' is already pending.`));
    if (Date.now() >= deadline) return Promise.reject(commandTimeout());
    return new Promise((resolve, reject) => {
        const timeoutId = window.setTimeout(() => {
            pending.delete(requestId);
            postCancel(requestId);
            reject(commandTimeout());
        }, Math.max(0, deadline - Date.now()));
        pending.set(requestId, { resolve, reject, timeoutId });
        window.parent.postMessage(withWindowMessageChannel(ACP_MESSAGE_CHANNEL, {
            event: FRONTEND_AGENT_EXECUTE_COMMAND,
            requestId,
            command,
            args,
            deadline,
        }), parentOrigin);
    });
};

export const cancelFrontendCommand = (targetRequestId: string): void => {
    const command = pending.get(targetRequestId);
    if (command) {
        finish(targetRequestId);
        command.reject(commandCancelled());
    }
    postCancel(targetRequestId);
};

const postCancel = (targetRequestId: string): void => {
    window.parent.postMessage(withWindowMessageChannel(ACP_MESSAGE_CHANNEL, {
        event: FRONTEND_AGENT_CANCEL_COMMAND,
        requestId: crypto.randomUUID(),
        targetRequestId,
    }), parentOrigin);
};

const finish = (requestId: string): void => {
    const command = pending.get(requestId);
    if (!command) return;
    window.clearTimeout(command.timeoutId);
    pending.delete(requestId);
};

const commandTimeout = (): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.TIMEOUT,
    message: 'The frontend command exceeded its deadline.',
    retryable: true,
});

const commandCancelled = (): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.CANCELLED,
    message: 'The frontend command was cancelled.',
    retryable: true,
});
