/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { observeTimeline, registerTimelineCommands } from './timelineController';

const moduleAgentCommandClient = new ModuleAgentCommandClient({
    moduleId: 'Timeline',
    observe: observeTimeline,
});

registerTimelineCommands(moduleAgentCommandClient);

let stopClient: (() => void) | undefined;

export const startTimelineAgentRuntime = (): void => {
    if (stopClient) return;
    stopClient = moduleAgentCommandClient.start();
};

export const stopTimelineAgentRuntime = (): void => {
    stopClient?.();
    stopClient = undefined;
};
