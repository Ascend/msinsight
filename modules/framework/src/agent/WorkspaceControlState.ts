/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export const CONTROL_FADE_MS = 350;
export const CONTROL_BREATH_MS = 3200;
export const INPUT_FEEDBACK_MS = 1200;
const IDLE_GRACE_MS = 5000;

export interface WorkspaceControlSnapshot {
    readonly phase: 'idle' | 'active' | 'input' | 'fading';
    readonly color: 'active' | 'input';
    readonly executing: boolean;
    readonly epoch: number;
}

export class WorkspaceControlState {
    private readonly executions = new Map<string, AbortSignal>();
    private readonly listeners = new Set<() => void>();
    private snapshot: WorkspaceControlSnapshot = { phase: 'idle', color: 'active', executing: false, epoch: 0 };
    private hideTimer?: number;
    private inputTimer?: number;
    private visibleSince = 0;
    private lastInputAt?: number;
    private inputRevision = 0;

    readonly getSnapshot = (): WorkspaceControlSnapshot => this.snapshot;
    getInputRevision(since?: number): number {
        if (since !== undefined && (this.lastInputAt === undefined || this.lastInputAt < since)) return 0;
        return this.inputRevision;
    }

    readonly getInputEpoch = (): number | null => this.snapshot.phase === 'idle' ? null : this.snapshot.epoch;

    readonly subscribe = (listener: () => void): (() => void) => {
        this.listeners.add(listener);
        return () => { this.listeners.delete(listener); };
    };

    begin(requestId: string, signal: AbortSignal): () => void {
        const finish = (): void => {
            signal.removeEventListener('abort', finish);
            // A late finally from a detached Agent must not release a newer request using the same ID.
            if (this.executions.get(requestId) !== signal) return;
            this.executions.delete(requestId);
            if (this.executions.size !== 0) return;
            this.scheduleFade();
            this.publish(this.snapshot.phase, this.snapshot.color);
        };
        const first = this.executions.size === 0;
        this.executions.set(requestId, signal);
        signal.addEventListener('abort', finish, { once: true });
        if (first) {
            window.clearTimeout(this.hideTimer);
            const newWindow = this.snapshot.phase === 'idle';
            if (newWindow) {
                this.visibleSince = Date.now();
                this.lastInputAt = undefined;
            }
            const phase = this.inputTimer === undefined ? 'active' : 'input';
            this.publish(phase, phase, newWindow ? this.snapshot.epoch + 1 : this.snapshot.epoch);
        }
        return finish;
    }

    recordInput(epoch: number = this.snapshot.epoch, occurredAt: number = Date.now()): void {
        const currentEpoch = this.getInputEpoch();
        if (currentEpoch === null || epoch !== currentEpoch || !Number.isFinite(occurredAt)) return;
        const now = Date.now();
        const inputAt = Math.min(occurredAt, now);
        const remaining = inputAt + INPUT_FEEDBACK_MS - now;
        if (inputAt < this.visibleSince || remaining <= 0 ||
            (this.lastInputAt !== undefined && inputAt <= this.lastInputAt)) return;
        this.lastInputAt = inputAt;
        this.inputRevision++;
        window.clearTimeout(this.inputTimer);
        this.inputTimer = window.setTimeout(() => {
            this.inputTimer = undefined;
            if (this.snapshot.phase === 'input') this.publish('active', 'active');
        }, remaining);
        // A human action can change the workspace until the indicator disappears.
        // Keep an ongoing fade on schedule; only another Agent command can renew it.
        if (this.snapshot.color !== 'input') {
            this.publish(this.snapshot.phase === 'fading' ? 'fading' : 'input', 'input');
        }
    }

    dispose(): void {
        window.clearTimeout(this.hideTimer);
        window.clearTimeout(this.inputTimer);
        this.inputTimer = undefined;
        this.executions.clear();
        if (this.snapshot.phase !== 'idle') this.publish('idle', 'active');
        this.listeners.clear();
    }

    private scheduleFade(): void {
        this.hideTimer = window.setTimeout(() => {
            this.hideTimer = window.setTimeout(() => {
                window.clearTimeout(this.inputTimer);
                this.inputTimer = undefined;
                this.publish('idle', 'active');
            }, CONTROL_FADE_MS);
            this.publish('fading', this.snapshot.color);
        }, IDLE_GRACE_MS);
    }

    private publish(phase: WorkspaceControlSnapshot['phase'], color: WorkspaceControlSnapshot['color'], epoch = this.snapshot.epoch): void {
        this.snapshot = { phase, color, epoch, executing: this.executions.size !== 0 };
        this.listeners.forEach(listener => {
            try {
                listener();
            } catch (error) {
                // Feedback subscribers must never interrupt the command they describe.
                globalThis.console.error('Workspace control subscriber failed.', error);
            }
        });
    }
}
