/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { useCallback, useLayoutEffect, useRef, useState, type RefObject, type UIEventHandler, type WheelEventHandler } from 'react';
import type { ChatMessage, ConversationNotice } from '../types';

const BOTTOM_THRESHOLD = 80;

interface ChatScrollOptions {
    containerRef: RefObject<HTMLDivElement>;
    messages: ChatMessage[];
    notices: ConversationNotice[];
    sessionId?: string;
    isDraftSession?: boolean;
    scrollRequest: number;
}

export const useChatScroll = ({ containerRef, messages, notices, sessionId, isDraftSession, scrollRequest }: ChatScrollOptions): {
    contentRef: RefObject<HTMLDivElement>;
    showLatest: boolean;
    onScroll: UIEventHandler<HTMLElement>;
    onWheel: WheelEventHandler<HTMLElement>;
    scrollToLatest: () => void;
} => {
    const contentRef = useRef<HTMLDivElement>(null);
    const following = useRef(true);
    const lastScrollTop = useRef(0);
    const [showLatest, setShowLatest] = useState(false);
    const hasMessages = messages.length > 0;

    const scrollToBottom = useCallback((): void => {
        const container = containerRef.current;
        if (!container) return;
        container.scrollTop = Math.max(0, container.scrollHeight - container.clientHeight);
        lastScrollTop.current = container.scrollTop;
    }, [containerRef]);

    const scrollToLatest = useCallback((): void => {
        following.current = true;
        setShowLatest(false);
        scrollToBottom();
    }, [scrollToBottom]);

    const updateLatestVisibility = useCallback((): void => {
        const container = containerRef.current;
        if (!container) return;
        const distance = container.scrollHeight - container.clientHeight - Math.max(0, container.scrollTop);
        setShowLatest(!following.current && distance > BOTTOM_THRESHOLD);
    }, [containerRef]);

    useLayoutEffect(() => {
        scrollToLatest();
    }, [sessionId, isDraftSession, scrollRequest, scrollToLatest]);

    useLayoutEffect(() => {
        if (following.current) scrollToBottom();
        updateLatestVisibility();
    }, [messages, notices, scrollToBottom, updateLatestVisibility]);

    useLayoutEffect(() => {
        const container = containerRef.current;
        const content = contentRef.current;
        if (!container || !content || typeof ResizeObserver === 'undefined') return;
        const observer = new ResizeObserver(() => {
            if (following.current) scrollToBottom();
            updateLatestVisibility();
        });
        observer.observe(container);
        observer.observe(content);
        return () => observer.disconnect();
    }, [containerRef, hasMessages, scrollToBottom, updateLatestVisibility]);

    const onScroll: UIEventHandler<HTMLElement> = (event) => {
        const container = event.currentTarget;
        const top = Math.max(0, container.scrollTop);
        const distance = container.scrollHeight - container.clientHeight - top;
        if (container.scrollHeight <= container.clientHeight) {
            following.current = true;
        } else if (top < lastScrollTop.current) {
            following.current = false;
        } else if (top > lastScrollTop.current && distance <= BOTTOM_THRESHOLD) {
            following.current = true;
        }
        lastScrollTop.current = top;
        updateLatestVisibility();
    };

    const onWheel: WheelEventHandler<HTMLElement> = (event) => {
        // Pause before the scroll event so an arriving chunk cannot undo the user's upward scroll.
        if (event.deltaY < 0 && event.currentTarget.scrollHeight > event.currentTarget.clientHeight) {
            following.current = false;
            updateLatestVisibility();
        }
    };

    return { contentRef, showLatest, onScroll, onWheel, scrollToLatest };
};
