/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { useLayoutEffect, type RefObject } from 'react';
import { attachWorkspaceInputFrame, createWorkspaceInputCapture, type WorkspaceInputState } from '@insight/lib/WorkspaceInput';

// Both the panel (including its drag handle) and the toolbar toggle operate on chat.
const isChatInput = (event: Event): boolean => event.composedPath().some(
    node => node instanceof Element && node.matches('.acp-session-wrapper, [data-agent-chat-toggle]'),
);

export function useWorkspaceInput(
    state: WorkspaceInputState,
    body: RefObject<HTMLDivElement>,
    activeModule: string,
    modules: ReadonlyArray<{ name: string }>,
): void {
    useLayoutEffect(() => {
        const capture = createWorkspaceInputCapture(document, (_, epoch, at) => state.recordInput(epoch, at), isChatInput);
        const sync = (): void => capture.setEpoch(state.getInputEpoch());
        const unwatch = state.subscribe(sync);
        sync();
        return () => { unwatch(); capture.dispose(); };
    }, [state]);

    useLayoutEffect(() => {
        if (!modules.some(module => module.name === activeModule)) return;
        const container = document.getElementById(activeModule);
        if (!container || !body.current?.contains(container)) return;
        if (container instanceof HTMLIFrameElement) return attachWorkspaceInputFrame(container, state);

        // VS Code can inject or replace an about:blank iframe inside its module div.
        let current: HTMLIFrameElement | null;
        let stop: (() => void) | undefined;
        const sync = (): void => {
            const frame = container.querySelector('iframe');
            if (frame === current) return;
            stop?.();
            current = frame;
            stop = frame ? attachWorkspaceInputFrame(frame, state) : undefined;
        };
        sync();
        const observer = new MutationObserver(sync);
        observer.observe(container, { childList: true });
        return () => { observer.disconnect(); stop?.(); };
    }, [state, body, activeModule, modules]);
}
