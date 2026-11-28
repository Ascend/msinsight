/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { connectWorkspaceInputChild, isWorkspaceInputKind } from '@insight/lib/WorkspaceInput';

const BODY_INPUT_READY = 'insightReport/inputReady';

// The body has an opaque origin. Register before report HTML runs and retain the
// sending port inside this closure. Only an input category, epoch and time leave it.
export const reportBodyInputScript = `<script>
(() => {
    const channel = new MessageChannel();
    const send = channel.port1.postMessage.bind(channel.port1);
    const modifiers = new Set(['Alt', 'AltGraph', 'Control', 'Shift', 'Meta', 'CapsLock', 'NumLock', 'ScrollLock', 'Fn', 'FnLock', 'Symbol', 'SymbolLock']);
    const pointers = new Set();
    let epoch = null;
    let lastSent = 0;
    let pending;
    let timer;
    function flush() {
        if (pending) {
            send(pending);
            lastSent = Date.now();
        }
        clearTimeout(timer);
        timer = undefined;
        pending = undefined;
    }
    function record(kind) {
        if (epoch === null) return;
        const occurredAt = Date.now();
        pending = { kind, epoch, occurredAt };
        const delay = 50 - (occurredAt - lastSent);
        if ((kind === 'wheel' || kind === 'drag') && delay > 0) {
            if (timer === undefined) timer = setTimeout(flush, delay);
        } else {
            flush();
        }
    }
    channel.port1.onmessage = event => {
        clearTimeout(timer);
        timer = undefined;
        pending = undefined;
        lastSent = 0;
        epoch = Number.isSafeInteger(event.data) && event.data >= 0 ? event.data : null;
    };
    const observe = (type, handle) => addEventListener(type, event => {
        if (event.isTrusted) handle(event);
    }, { capture: true, passive: true });
    observe('pointerdown', event => { pointers.add(event.pointerId); record('pointer'); });
    observe('pointermove', event => {
        if (!event.buttons) pointers.delete(event.pointerId);
        else if (pointers.has(event.pointerId)) record('drag');
    });
    observe('pointerup', event => pointers.delete(event.pointerId));
    observe('pointercancel', event => pointers.delete(event.pointerId));
    addEventListener('blur', () => pointers.clear());
    observe('wheel', () => record('wheel'));
    observe('keydown', event => { if (!modifiers.has(event.key)) record('key'); });
    observe('beforeinput', event => {
        const target = event.target;
        if (target instanceof HTMLElement && (target.isContentEditable || target.matches('input, textarea'))) record('edit');
    });
    parent.postMessage('${BODY_INPUT_READY}', '*', [channel.port2]);
})();
</script>`;

/** The report viewer alone may forward activity to its registered Framework frame. */
export const connectReportInput = (frame: HTMLIFrameElement | null): (() => void) => {
    let port: MessagePort | undefined;
    let epoch: number | null = null;
    const host = connectWorkspaceInputChild((nextEpoch) => {
        epoch = nextEpoch;
        port?.postMessage(nextEpoch);
    });
    const onReady = (event: MessageEvent): void => {
        if (!frame?.contentWindow || event.source !== frame.contentWindow || event.origin !== 'null' ||
            event.data !== BODY_INPUT_READY || event.ports.length !== 1 || port) return;
        port = event.ports[0];
        port.onmessage = ({ data }: MessageEvent): void => {
            if (epoch === null || data?.epoch !== epoch || !isWorkspaceInputKind(data.kind) || !Number.isFinite(data.occurredAt)) return;
            host.report(data.kind, epoch, data.occurredAt);
        };
        port.postMessage(epoch);
    };
    window.addEventListener('message', onReady);
    return () => {
        window.removeEventListener('message', onReady);
        if (port) {
            port.onmessage = null;
            port.close();
        }
        host.dispose();
    };
};
