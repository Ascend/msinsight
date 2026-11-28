/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

export type WorkspaceInputKind = 'pointer' | 'drag' | 'wheel' | 'key' | 'edit';
export const isWorkspaceInputKind = (kind: unknown): kind is WorkspaceInputKind => (
    kind === 'pointer' || kind === 'drag' || kind === 'wheel' || kind === 'key' || kind === 'edit'
);

const modifierKeys = new Set([
    'Alt', 'AltGraph', 'Control', 'Meta', 'Shift', 'CapsLock', 'NumLock', 'ScrollLock', 'Fn', 'FnLock', 'Symbol', 'SymbolLock',
]);
const continuousIntervalMs = 50;
type EmitInput = (kind: WorkspaceInputKind, epoch: number, occurredAt: number) => void;

export interface WorkspaceInputCapture {
    setEpoch: (epoch: number | null) => void;
    dispose: () => void;
}

// Emit the leading and trailing samples of a burst. Keep the actual input time
// so that the last wheel/drag sample doesn't extend the indicator's lifetime.
export function createWorkspaceInputCapture(doc: Document, emit: EmitInput, exclude?: (event: Event) => boolean): WorkspaceInputCapture {
    const pointers = new Set<number>();
    let epoch: number | null = null;
    let timer: ReturnType<typeof setTimeout> | undefined;
    let lastSentAt = -Infinity;
    let pending: { kind: WorkspaceInputKind; at: number } | undefined;

    const flush = (): void => {
        clearTimeout(timer);
        timer = undefined;
        if (!pending || epoch === null) return;
        const { kind, at } = pending;
        pending = undefined;
        lastSentAt = Date.now();
        emit(kind, epoch, at);
    };
    const report = (kind: WorkspaceInputKind): void => {
        if (epoch === null) return;
        const at = Date.now();
        pending = { kind, at };
        const remaining = continuousIntervalMs - (at - lastSentAt);
        if ((kind === 'drag' || kind === 'wheel') && remaining > 0) {
            timer ??= setTimeout(flush, remaining);
        } else {
            flush();
        }
    };
    const capture = (event: Event): void => {
        if (!event.isTrusted) return;
        const pointer = event as PointerEvent;
        switch (event.type) {
            case 'pointerup':
            case 'pointercancel':
                pointers.delete(pointer.pointerId);
                return;
            case 'pointerdown':
                if (exclude?.(event)) {
                    pointers.delete(pointer.pointerId);
                } else {
                    pointers.add(pointer.pointerId);
                    report('pointer');
                }
                return;
            case 'pointermove':
                if (!pointer.buttons) pointers.delete(pointer.pointerId);
                if (pointers.has(pointer.pointerId) && !exclude?.(event)) report('drag');
                return;
            default:
                if (exclude?.(event)) return;
                if (event.type === 'wheel') report('wheel');
                if (event.type === 'keydown' && !modifierKeys.has((event as KeyboardEvent).key)) report('key');
                if (event.type === 'beforeinput' && event.target instanceof HTMLElement &&
                    (event.target.isContentEditable || event.target.matches('input, textarea'))) report('edit');
        }
    };
    // Track where a press began even before a command starts. Movement, wheel
    // and editing listeners are needed only while the workspace indicator is visible.
    const pointerTypes = ['pointerdown', 'pointerup', 'pointercancel'];
    const activeTypes = ['pointermove', 'wheel', 'keydown', 'beforeinput'];
    const options = { capture: true, passive: true };
    const resetPointers = (): void => pointers.clear();
    for (const type of pointerTypes) doc.addEventListener(type, capture, options);
    doc.defaultView?.addEventListener('blur', resetPointers);
    const setEpoch = (next: number | null): void => {
        if (epoch === next) return;
        clearTimeout(timer);
        timer = undefined;
        pending = undefined;
        lastSentAt = -Infinity;
        if (next === null) {
            for (const type of activeTypes) doc.removeEventListener(type, capture, true);
        } else if (epoch === null) {
            for (const type of activeTypes) doc.addEventListener(type, capture, options);
        }
        epoch = next;
    };
    return {
        setEpoch,
        dispose: () => {
            setEpoch(null);
            for (const type of pointerTypes) doc.removeEventListener(type, capture, true);
            doc.defaultView?.removeEventListener('blur', resetPointers);
            pointers.clear();
        },
    };
}
