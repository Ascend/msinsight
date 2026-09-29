/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { observeCommunication, registerCommunicationCommands, setCommunicationAgentState } from './communicationController';

const client = new ModuleAgentCommandClient({
    moduleId: 'Communication',
    observe: observeCommunication,
});

registerCommunicationCommands(client);

let stopClient: (() => void) | undefined;

export const startCommunicationAgentRuntime = (): void => {
    if (stopClient) return;
    stopClient = client.start();
};

export const stopCommunicationAgentRuntime = (): void => {
    stopClient?.();
    stopClient = undefined;
    setCommunicationAgentState(undefined);
};
