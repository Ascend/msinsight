/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { withAbortSignal } from '../FrontendAgentCommand';
import { AgentTableError, TABLE_ERROR_CODES } from './errors';
import { stableStringify } from './utils';
import type {
    TableCommandRequest,
    TableCommandResult,
    TableController,
    TableControllerRegistration,
    TableObservation,
    TableStableSnapshot,
    TransitionContext,
} from './types';

interface RegistryEntry {
    controller: TableController;
    revision: number;
    snapshot: TableStableSnapshot;
    snapshotKey: string;
    commandKey: string;
    queue: Promise<void>;
    queuedRequestIds: Set<string>;
    cancelledRequestIds: Set<string>;
    active?: { requestId: string; controller: AbortController };
    unsubscribe?: () => void;
}

export class TableControllerRegistry {
    private readonly entries = new Map<string, RegistryEntry>();
    private readonly commandSubscribers = new Set<() => void>();

    register(controller: TableController): TableControllerRegistration {
        const targetId = createTargetId();
        const snapshot = controller.getSnapshot();
        const entry: RegistryEntry = {
            controller,
            revision: 1,
            snapshot,
            snapshotKey: stableStringify(snapshot),
            commandKey: commandKey(controller, snapshot),
            queue: Promise.resolve(),
            queuedRequestIds: new Set(),
            cancelledRequestIds: new Set(),
        };
        entry.unsubscribe = controller.subscribeStable?.((next) => this.commitSnapshot(entry, next));
        this.entries.set(targetId, entry);
        this.notifyCommandsChanged();
        return {
            targetId,
            unregister: () => this.unregister(targetId),
        };
    }

    subscribeCommandsChanged(listener: () => void): () => void {
        this.commandSubscribers.add(listener);
        return () => {
            this.commandSubscribers.delete(listener);
        };
    }

    listCommandIds(): TableCommandRequest['commandId'][] {
        const commandIds = new Set<TableCommandRequest['commandId']>();
        this.entries.forEach((entry) => {
            if (!entry.controller.getAvailability().visible) return;
            entry.controller.getSnapshot().capabilities.forEach(commandId => commandIds.add(commandId));
        });
        return [...commandIds];
    }

    observe(): TableObservation[] {
        const observations: TableObservation[] = [];
        this.entries.forEach((entry, targetId) => {
            const availability = entry.controller.getAvailability();
            if (!availability.visible) return;
            this.commitSnapshot(entry, entry.controller.getSnapshot());
            observations.push({
                protocolVersion: 1,
                targetId,
                tableKey: entry.controller.tableKey,
                title: entry.controller.title,
                revision: entry.revision,
                availability,
                ...entry.snapshot,
            });
        });
        return observations;
    }

    invoke(request: TableCommandRequest, signal?: AbortSignal): Promise<TableCommandResult> {
        const entry = this.entries.get(request.targetId);
        if (!entry) return Promise.reject(targetUnavailable(request.targetId));
        if (signal?.aborted) return Promise.reject(signal.reason ?? cancelledError());
        entry.queuedRequestIds.add(request.requestId);
        const abort = (): void => this.cancel(request.requestId);
        signal?.addEventListener('abort', abort, { once: true });
        const run = (): Promise<TableCommandResult> => {
            entry.queuedRequestIds.delete(request.requestId);
            if (entry.cancelledRequestIds.delete(request.requestId)) return Promise.reject(cancelledError());
            return this.execute(entry, request, signal);
        };
        const result = entry.queue.then(run, run);
        entry.queue = result.then(() => undefined, () => undefined);
        return result.finally(() => signal?.removeEventListener('abort', abort));
    }

    cancel(requestId: string): void {
        this.entries.forEach((entry) => {
            if (entry.queuedRequestIds.has(requestId)) entry.cancelledRequestIds.add(requestId);
            if (entry.active?.requestId !== requestId) return;
            entry.active.controller.abort(cancelledError());
            entry.controller.cancel?.(requestId);
        });
    }

    dispose(): void {
        [...this.entries.keys()].forEach((targetId) => this.unregister(targetId));
    }

    private unregister(targetId: string): void {
        const entry = this.entries.get(targetId);
        if (!entry) return;
        if (entry.active) {
            entry.active.controller.abort(new AgentTableError({
                code: TABLE_ERROR_CODES.TARGET_UNAVAILABLE,
                message: 'The table was unmounted while a command was running.',
                retryable: true,
            }));
            entry.controller.cancel?.(entry.active.requestId);
        }
        entry.unsubscribe?.();
        entry.queuedRequestIds.forEach(requestId => entry.cancelledRequestIds.add(requestId));
        entry.queuedRequestIds.clear();
        this.entries.delete(targetId);
        this.notifyCommandsChanged();
    }

    private notifyCommandsChanged(): void {
        this.commandSubscribers.forEach(listener => listener());
    }

    private async execute(entry: RegistryEntry, request: TableCommandRequest, externalSignal?: AbortSignal): Promise<TableCommandResult> {
        if (externalSignal?.aborted) throw externalSignal.reason ?? cancelledError();
        const availability = entry.controller.getAvailability();
        if (!availability.visible) throw targetUnavailable(request.targetId);
        if (!availability.ready) throw tableError(TABLE_ERROR_CODES.NOT_READY, 'The table is not ready.', true);
        if (availability.busy) throw tableError(TABLE_ERROR_CODES.BUSY, 'The table is busy.', true);
        if (Date.now() >= request.deadline) throw timeoutError();

        this.commitSnapshot(entry, entry.controller.getSnapshot());
        if (request.expectedRevision !== entry.revision) {
            throw new AgentTableError({
                code: TABLE_ERROR_CODES.STATE_STALE,
                message: 'The table state changed after it was observed.',
                retryable: true,
                details: {
                    expectedRevision: request.expectedRevision,
                    currentRevision: entry.revision,
                },
                state: entry.snapshot.state,
            });
        }

        const transitionController = new AbortController();
        const cancelTransition = (reason: unknown): void => {
            if (transitionController.signal.aborted) return;
            transitionController.abort(reason);
            entry.controller.cancel?.(request.requestId);
        };
        const abortFromExternal = (): void => cancelTransition(externalSignal?.reason ?? cancelledError());
        externalSignal?.addEventListener('abort', abortFromExternal, { once: true });
        const timeout = window.setTimeout(() => cancelTransition(timeoutError()), Math.max(0, request.deadline - Date.now()));
        entry.active = { requestId: request.requestId, controller: transitionController };
        const context: TransitionContext = {
            requestId: request.requestId,
            transitionId: crypto.randomUUID(),
            source: 'agent',
            deadline: request.deadline,
            signal: transitionController.signal,
        };

        try {
            const execution = await withAbortSignal(entry.controller.execute(request, context), transitionController.signal, cancelledError);
            const revisionBeforeCommit = entry.revision;
            if (execution.snapshot) this.commitSnapshot(entry, execution.snapshot);
            if (request.commandId === 'table.refresh' && entry.revision === revisionBeforeCommit) entry.revision += 1;
            if (execution.targetStatus === 'unavailable') {
                return {
                    status: 'completed',
                    targetStatus: 'unavailable',
                    effect: execution.effect,
                    requiresObserve: execution.requiresObserve ?? true,
                };
            }
            return {
                status: 'completed',
                targetId: request.targetId,
                targetStatus: 'available',
                revision: entry.revision,
                noOp: execution.changed === false,
                state: entry.snapshot.state,
                result: execution.result,
                effect: execution.effect,
                requiresObserve: execution.requiresObserve,
            };
        } finally {
            window.clearTimeout(timeout);
            externalSignal?.removeEventListener('abort', abortFromExternal);
            if (entry.active?.requestId === request.requestId) entry.active = undefined;
        }
    }

    private commitSnapshot(entry: RegistryEntry, snapshot: TableStableSnapshot): void {
        const nextKey = stableStringify(snapshot);
        const nextCommandKey = commandKey(entry.controller, snapshot);
        if (nextCommandKey !== entry.commandKey) {
            entry.commandKey = nextCommandKey;
            this.notifyCommandsChanged();
        }
        if (nextKey === entry.snapshotKey) return;
        entry.snapshot = snapshot;
        entry.snapshotKey = nextKey;
        entry.revision += 1;
    }
}

export const defaultTableControllerRegistry = new TableControllerRegistry();

const createTargetId = (): string => `table-${crypto.randomUUID()}`;

const commandKey = (controller: TableController, snapshot: TableStableSnapshot): string => {
    const availability = controller.getAvailability();
    return stableStringify({
        visible: availability.visible,
        capabilities: [...snapshot.capabilities].sort(),
    });
};

const targetUnavailable = (targetId: string): AgentTableError => tableError(
    TABLE_ERROR_CODES.TARGET_UNAVAILABLE,
    `Table target '${targetId}' is unavailable.`,
    true,
);

const timeoutError = (): AgentTableError => tableError(
    TABLE_ERROR_CODES.COMMAND_TIMEOUT,
    'The table command exceeded its deadline.',
    true,
);

const cancelledError = (): AgentTableError => tableError(
    TABLE_ERROR_CODES.COMMAND_CANCELLED,
    'The table command was cancelled.',
    true,
);

const tableError = (code: string, message: string, retryable: boolean): AgentTableError => new AgentTableError({ code, message, retryable });
