/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { CommandDefinition, CommandErrorShape, JsonObject } from './types';

export const ACP_MESSAGE_CHANNEL = 'acpMessage';
export const MODULE_AGENT_MESSAGE_CHANNEL = 'moduleAgentMessage';

export const FRONTEND_AGENT_EXECUTE_COMMAND = 'frontendAgent/executeCommand';
export const FRONTEND_AGENT_CANCEL_COMMAND = 'frontendAgent/cancelCommand';
export const FRONTEND_AGENT_COMMAND_RESPONSE = 'frontendAgent/commandResponse';
export const FRONTEND_AGENT_COMMAND_ERROR = 'frontendAgent/commandError';

export const MODULE_AGENT_HELLO = 'moduleAgent/hello';
export const MODULE_AGENT_READY = 'moduleAgent/ready';
export const MODULE_AGENT_COMMANDS_CHANGED = 'moduleAgent/commandsChanged';
export const MODULE_AGENT_OBSERVE = 'moduleAgent/observe';
export const MODULE_AGENT_EXECUTE_COMMAND = 'moduleAgent/executeCommand';
export const MODULE_AGENT_CANCEL_COMMAND = 'moduleAgent/cancelCommand';
export const MODULE_AGENT_COMMAND_RESPONSE = 'moduleAgent/commandResponse';
export const MODULE_AGENT_COMMAND_ERROR = 'moduleAgent/commandError';

export interface FrontendAgentExecuteCommandMessage {
    event: typeof FRONTEND_AGENT_EXECUTE_COMMAND;
    requestId: string;
    command: string;
    args: JsonObject;
    deadline: number;
}

export interface FrontendAgentCancelCommandMessage {
    event: typeof FRONTEND_AGENT_CANCEL_COMMAND;
    requestId: string;
    targetRequestId: string;
}

export interface FrontendAgentCommandResponseMessage {
    event: typeof FRONTEND_AGENT_COMMAND_RESPONSE;
    requestId: string;
    result: unknown;
}

export interface FrontendAgentCommandErrorMessage {
    event: typeof FRONTEND_AGENT_COMMAND_ERROR;
    requestId: string;
    error: CommandErrorShape;
}

interface ModuleAgentMessageBase {
    moduleId: string;
    connectionToken: string;
}

export interface ModuleAgentHelloMessage extends ModuleAgentMessageBase {
    event: typeof MODULE_AGENT_HELLO;
}

export interface ModuleAgentReadyMessage extends ModuleAgentMessageBase {
    event: typeof MODULE_AGENT_READY;
}

export interface ModuleAgentCommandsChangedMessage extends ModuleAgentMessageBase {
    event: typeof MODULE_AGENT_COMMANDS_CHANGED;
    commands: CommandDefinition[];
}

export interface ModuleAgentObserveMessage extends ModuleAgentMessageBase {
    event: typeof MODULE_AGENT_OBSERVE;
    requestId: string;
    deadline: number;
}

export interface ModuleAgentExecuteCommandMessage extends ModuleAgentMessageBase {
    event: typeof MODULE_AGENT_EXECUTE_COMMAND;
    requestId: string;
    command: string;
    args: JsonObject;
    deadline: number;
}

export interface ModuleAgentCancelCommandMessage extends ModuleAgentMessageBase {
    event: typeof MODULE_AGENT_CANCEL_COMMAND;
    requestId: string;
    targetRequestId: string;
}

export interface ModuleAgentCommandResponseMessage extends ModuleAgentMessageBase {
    event: typeof MODULE_AGENT_COMMAND_RESPONSE;
    requestId: string;
    result: unknown;
}

export interface ModuleAgentCommandErrorMessage extends ModuleAgentMessageBase {
    event: typeof MODULE_AGENT_COMMAND_ERROR;
    requestId: string;
    error: CommandErrorShape;
}

export type FrontendAgentCommandRequestMessage = FrontendAgentExecuteCommandMessage | FrontendAgentCancelCommandMessage;
export type FrontendAgentCommandResultMessage = FrontendAgentCommandResponseMessage | FrontendAgentCommandErrorMessage;
export type ModuleAgentControllerMessage = ModuleAgentHelloMessage | ModuleAgentObserveMessage | ModuleAgentExecuteCommandMessage | ModuleAgentCancelCommandMessage;
export type ModuleAgentClientMessage = ModuleAgentReadyMessage | ModuleAgentCommandsChangedMessage | ModuleAgentCommandResponseMessage | ModuleAgentCommandErrorMessage;
