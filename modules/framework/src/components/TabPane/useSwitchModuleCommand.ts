/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { useEffect, useLayoutEffect, useRef, useState, type Dispatch, type SetStateAction } from 'react';
import { COMMAND_ERROR_CODES, CommandError, type CommandDefinition } from '@insight/lib/FrontendAgentCommand';
import type { FrontendAgentCommandController } from '../../agent/frontendAgentCommandController/FrontendAgentCommandController';

export const SWITCH_MODULE_DEFINITION: CommandDefinition = {
    name: 'framework.switchModule',
    title: 'Switch active module',
    description: 'Switch to a module in observe.app.availableModules. Completes after the active tab and command routing are updated, not after module loading.',
    approval: 'none',
    inputSchema: {
        type: 'object',
        properties: { moduleId: { type: 'string', minLength: 1 } },
        required: ['moduleId'],
        additionalProperties: false,
    },
};

interface PendingSwitch {
    revision: number;
    moduleId: string;
    signal: AbortSignal;
    resolve: (value: { activeModule: string }) => void;
    reject: (reason: unknown) => void;
    cleanup: () => void;
}

const commandError = (code: string, message: string): CommandError => new CommandError({ code, message, retryable: true });
const cancelled = (): CommandError => commandError(COMMAND_ERROR_CODES.CANCELLED, 'Module switching was cancelled.');

export function useSwitchModuleCommand(
    controller: FrontendAgentCommandController,
    activeModule: string,
    selectableModuleIds: string[],
    setActiveModule: Dispatch<SetStateAction<string>>,
): void {
    const committed = useRef({ activeModule, selectableModuleIds });
    const pending = useRef<PendingSwitch>();
    const nextRevision = useRef(0);
    const [revision, setRevision] = useState(0);

    // A revision forces a commit even for an idempotent switch. Only this commit may
    // acknowledge its request, and routing must be updated before its promise resolves.
    useLayoutEffect(() => {
        committed.current = { activeModule, selectableModuleIds };
        controller.setActiveModule(activeModule);
        const request = pending.current;
        if (!request || request.revision !== revision) return;
        pending.current = undefined;
        request.cleanup();
        if (request.signal.aborted) {
            request.reject(request.signal.reason ?? cancelled());
        } else if (activeModule !== request.moduleId || !selectableModuleIds.includes(request.moduleId)) {
            request.reject(new CommandError({
                code: COMMAND_ERROR_CODES.UNAVAILABLE,
                message: 'Module selection changed before switching completed.',
                retryable: true,
                details: { availableModules: selectableModuleIds },
            }));
        } else {
            request.resolve({ activeModule });
        }
    });

    useEffect(() => {
        const unregister = controller.registerGlobalCommand(SWITCH_MODULE_DEFINITION, (args, context) => {
            if (Object.keys(args).some(key => key !== 'moduleId') || typeof args.moduleId !== 'string' || !args.moduleId.trim()) {
                throw new CommandError({
                    code: COMMAND_ERROR_CODES.INVALID,
                    message: 'Expected only moduleId, a non-empty string.',
                    retryable: false,
                });
            }
            const moduleId = args.moduleId.trim();
            const availableModules = committed.current.selectableModuleIds;
            if (!availableModules.includes(moduleId)) {
                throw new CommandError({
                    code: COMMAND_ERROR_CODES.UNAVAILABLE,
                    message: `Module '${moduleId}' is not available in the current tab menu.`,
                    retryable: true,
                    details: { availableModules },
                });
            }
            if (context.signal.aborted) throw context.signal.reason ?? cancelled();
            // Do not let React's batching collapse two targets and acknowledge the wrong request.
            if (pending.current) throw commandError(COMMAND_ERROR_CODES.BUSY, 'Another module switch is awaiting commit.');
            return new Promise<{ activeModule: string }>((resolve, reject) => {
                const onAbort = (): void => {
                    if (pending.current !== request) return;
                    pending.current = undefined;
                    request.cleanup();
                    // React may have eagerly evaluated the queued updater already. Undo
                    // only our still-uncommitted target, never a different UI selection.
                    const previousModule = committed.current.activeModule;
                    setActiveModule(previous => previous === moduleId ? previousModule : previous);
                    reject(context.signal.reason ?? cancelled());
                };
                const request: PendingSwitch = {
                    revision: ++nextRevision.current,
                    moduleId,
                    signal: context.signal,
                    resolve,
                    reject,
                    cleanup: () => context.signal.removeEventListener('abort', onAbort),
                };
                pending.current = request;
                context.signal.addEventListener('abort', onAbort, { once: true });
                // A cancelled queued update must not overwrite a newer request. Cancellation
                // cannot roll back navigation that React has already committed.
                setActiveModule(previous => pending.current === request && !context.signal.aborted ? moduleId : previous);
                setRevision(request.revision);
            });
        });
        return () => {
            unregister();
            const request = pending.current;
            pending.current = undefined;
            request?.cleanup();
            request?.reject(commandError(COMMAND_ERROR_CODES.CONNECTION_LOST, 'The tab pane was unmounted.'));
        };
    }, [controller, setActiveModule]);
}
