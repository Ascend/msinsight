/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { getWindowMessageRouter, isWindowMessageChannel, parentWindowMessageOrigin } from '../WindowMessageRouter';
import { createWorkspaceInputCapture, isWorkspaceInputKind, type WorkspaceInputCapture, type WorkspaceInputKind } from './capture';

export { createWorkspaceInputCapture, isWorkspaceInputKind, type WorkspaceInputKind } from './capture';

const channel = 'workspaceInput';
const filter = isWindowMessageChannel(channel);
let nextBinding = 0;

export interface WorkspaceInputState {
    subscribe: (callback: () => void) => () => void;
    getInputEpoch: () => number | null;
    recordInput: (epoch?: number, occurredAt?: number) => void;
}

const validEpoch = (value: unknown): value is number => typeof value === 'number' && Number.isSafeInteger(value) && value >= 0;

// Chromium represents a local file URL as file:// but may deliver its
// MessageEvent with the opaque origin "null". It requires "*" for delivery.
const deliveryOrigin = (origin: string): string => origin === 'file://' || origin === 'null' ? '*' : origin;
const acceptsOrigin = (actual: string, expected: string | undefined): boolean =>
    actual === expected || (expected === 'file://' && actual === 'null');

// about:blank module frames (VS Code) inherit their owner's origin.
const frameOrigin = (frame: HTMLIFrameElement): string | undefined => {
    if (!frame.src || frame.src === 'about:blank' || frame.hasAttribute('srcdoc')) return frame.ownerDocument.location.origin;
    try {
        const url = new URL(frame.src, frame.ownerDocument.baseURI);
        return url.protocol === 'file:' ? 'file://' : url.origin;
    } catch {
        return undefined;
    }
};

// Attached only to the currently visible business or report frame. A navigation
// can preserve its WindowProxy; rotate the binding to discard old document input.
export function attachWorkspaceInputFrame(frame: HTMLIFrameElement, state: WorkspaceInputState): () => void {
    let binding = String(++nextBinding);
    let epoch = state.getInputEpoch();
    const send = (event: 'listen' | 'detach' = 'listen'): void => {
        const origin = frameOrigin(frame);
        if (!origin || !frame.isConnected) return;
        try {
            frame.contentWindow?.postMessage({ channel, event, binding, epoch }, deliveryOrigin(origin));
        } catch {
            // A closing or navigating frame must not interrupt the business command.
        }
    };
    const onLoad = (): void => {
        binding = String(++nextBinding);
        send();
    };
    const unsubscribe = getWindowMessageRouter().subscribe(event => {
        if (!frame.isConnected || event.source !== frame.contentWindow || !acceptsOrigin(event.origin, frameOrigin(frame))) return;
        const message = event.data;
        if (message.event === 'ready') {
            send();
        } else if (message.event === 'input' && message.binding === binding && validEpoch(message.epoch) &&
            message.epoch === epoch && isWorkspaceInputKind(message.kind) && Number.isFinite(message.occurredAt)) {
            state.recordInput(message.epoch, message.occurredAt);
        }
    }, filter);
    const unwatch = state.subscribe(() => {
        const next = state.getInputEpoch();
        if (next === epoch) return;
        epoch = next;
        send();
    });
    frame.addEventListener('load', onLoad);
    send();
    return () => {
        unsubscribe();
        unwatch();
        frame.removeEventListener('load', onLoad);
        epoch = null;
        send('detach');
    };
}

export interface WorkspaceInputChild {
    report: (kind: WorkspaceInputKind, epoch: number, occurredAt?: number) => void;
    dispose: () => void;
}

export function connectWorkspaceInputChild(onEpoch?: (epoch: number | null) => void): WorkspaceInputChild {
    const parent = window.parent;
    const expectedOrigin = parentWindowMessageOrigin();
    const targetOrigin = deliveryOrigin(expectedOrigin);
    let epoch: number | null = null;
    let binding = '';
    let capture: WorkspaceInputCapture | undefined;
    const detach = (): void => {
        capture?.dispose();
        capture = undefined;
        epoch = null;
        binding = '';
    };
    const report = (kind: WorkspaceInputKind, inputEpoch: number, occurredAt = Date.now()): void => {
        if (epoch !== inputEpoch || !binding) return;
        parent.postMessage({ channel, event: 'input', binding, epoch, kind, occurredAt }, targetOrigin);
    };
    const unsubscribe = getWindowMessageRouter().subscribe(event => {
        if (event.source !== parent || (expectedOrigin !== '*' && !acceptsOrigin(event.origin, expectedOrigin))) return;
        const message = event.data;
        if (message.event === 'detach' && binding && message.binding === binding) {
            detach();
            onEpoch?.(null);
            return;
        }
        if (message.event !== 'listen' || typeof message.binding !== 'string' || !message.binding ||
            (message.epoch !== null && !validEpoch(message.epoch))) return;
        if (binding === message.binding && epoch === message.epoch) return;
        binding = message.binding;
        epoch = message.epoch as number | null;
        capture ??= createWorkspaceInputCapture(document, report);
        capture.setEpoch(epoch);
        onEpoch?.(epoch);
    }, filter);
    if (parent !== window) parent.postMessage({ channel, event: 'ready' }, targetOrigin);
    return {
        report,
        dispose: () => {
            detach();
            unsubscribe();
        },
    };
}
