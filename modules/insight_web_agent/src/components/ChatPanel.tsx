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
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import { useChatState } from '../hooks/useChatState';
import { useChatScroll } from '../hooks/useChatScroll';
import { Composer } from './Composer';
import { MessageList, ModelSwitchNotice } from './MessageList';
import { WelcomePanel } from './WelcomePanel';

const Container = styled.section`
    flex: 1 1 auto;
    min-height: 0;
    width: 100%;
    display: grid;
    grid-template-columns: minmax(0, 1fr);
    grid-template-rows: auto minmax(0, 1fr) auto;
    background: ${(props): string => props.theme.bgColor};
    overflow: hidden;

    .session-title-slot {
        grid-row: 1;
        min-width: 0;
        overflow: hidden;
        padding: 8px 16px 12px;
    }

    .session-title-slot:empty {
        display: none;
    }

    .session-title {
        width: 100%;
        max-width: 100%;
        min-width: 0;
        display: block;
        overflow: hidden;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 14px;
        font-weight: 600;
        line-height: 22px;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .messages {
        isolation: isolate;
        height: 100%;
        min-height: 0;
        display: flex;
        flex-direction: column;
        gap: 14px;
        overflow: auto;
        scrollbar-gutter: stable;
        padding: 0 16px;
    }

    .conversation-content {
        --composer-fade-height: 48px;
        position: relative;
        grid-row: 2;
        min-height: 0;
        overflow: hidden;
    }

    .conversation-content.has-messages::after {
        content: "";
        position: absolute;
        z-index: 1;
        right: 0;
        bottom: 0;
        left: 0;
        height: var(--composer-fade-height);
        background: linear-gradient(to bottom, transparent, ${(props): string => props.theme.bgColor});
        pointer-events: none;
    }

    .messages-content {
        flex-shrink: 0;
        min-width: 0;
        padding-bottom: var(--composer-fade-height);
    }

    .scroll-to-latest {
        position: absolute;
        z-index: 2;
        bottom: 12px;
        left: 50%;
        transform: translateX(-50%);
        display: flex;
        align-items: center;
        justify-content: center;
        width: 32px;
        height: 32px;
        padding: 0;
        border: 1px solid ${(props): string => props.theme.mode === 'dark' ? props.theme.borderColor : 'rgba(243, 243, 243, 1)'};
        border-radius: 50%;
        background: ${(props): string => props.theme.mode === 'dark' ? props.theme.bgColorLight : 'rgba(255, 255, 255, 1)'};
        color: ${(props): string => props.theme.textColorPrimary};
        box-shadow: 0 4px 12px 0 ${(props): string => props.theme.mode === 'dark' ? 'rgba(0, 0, 0, 0.36)' : 'rgba(0, 0, 0, 0.16)'};
        cursor: pointer;
        opacity: 0;
        visibility: hidden;
        pointer-events: none;
        transition: opacity 160ms ease, visibility 0s linear 160ms;
    }

    .scroll-to-latest[data-visible='true'] {
        opacity: 1;
        visibility: visible;
        pointer-events: auto;
        transition-delay: 0s;
    }

    .latest-output-dots {
        display: flex;
        align-items: center;
        gap: 3px;
        height: 16px;
    }

    .latest-output-dot {
        width: 4px;
        height: 4px;
        border-radius: 50%;
        background: currentColor;
        animation: latest-dot-wave 1100ms ease-in-out infinite;
        animation-play-state: paused;
    }

    .latest-output-dot:nth-child(2) { animation-delay: -160ms; }
    .latest-output-dot:nth-child(3) { animation-delay: -320ms; }

    .scroll-to-latest[data-visible='true'] .latest-output-dot { animation-play-state: running; }

    @keyframes latest-dot-wave {
        0%, 100% { opacity: 0.65; transform: translateY(2px); }
        50% { opacity: 0.35; transform: translateY(-2px); }
    }

    @keyframes latest-dot-pulse {
        0%, 100% { opacity: 0.65; }
        50% { opacity: 0.35; }
    }

    @media (prefers-reduced-motion: reduce) {
        .scroll-to-latest { transition: none; }
        .latest-output-dot { animation-name: latest-dot-pulse; }
    }

    .scroll-to-latest:hover {
        border-color: ${(props): string => props.theme.borderColorHover};
    }

    .scroll-to-latest:focus-visible {
        outline: 2px solid currentColor;
        outline-offset: 2px;
    }

    .composer-slot {
        grid-row: 3;
        min-width: 0;
    }

    .welcome-stack {
        height: 100%;
        min-height: 0;
        display: flex;
        flex-direction: column;
    }

    .welcome-stack > :first-child {
        flex: 1 1 auto;
        min-height: 0;
    }

    .welcome-notices {
        flex: 0 0 auto;
        padding: 0 16px 8px;
    }

`;

export const ChatPanel = (): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const { currentSessionId, isDraftSession, messages, messagesRef, notices = [], pendingPrompt, respondToPermission, sessions, scrollToLatestRequest = 0 } = useChatState();
    const { contentRef, showLatest, onScroll, onWheel, scrollToLatest } = useChatScroll({
        containerRef: messagesRef, messages, notices, sessionId: currentSessionId, isDraftSession, scrollRequest: scrollToLatestRequest,
    });
    const currentTitle = isDraftSession
        ? undefined
        : sessions.find((session) => session.sessionId === currentSessionId)?.title?.trim();
    const latestVisible = messages.length > 0 && showLatest;

    return (
        <Container>
            <div className="session-title-slot">
                {currentTitle ? <div className="session-title" title={currentTitle}>{currentTitle}</div> : null}
            </div>
            <div className={`conversation-content${messages.length ? ' has-messages' : ''}`}>
                {messages.length
                    ? <section className="messages" ref={messagesRef} onScroll={onScroll} onWheel={onWheel}>
                        <div className="messages-content" ref={contentRef}>
                            <MessageList
                                messages={messages}
                                notices={notices}
                                pendingPrompt={pendingPrompt}
                                onPermissionDecision={respondToPermission}
                            />
                        </div>
                    </section>
                    : <div className="welcome-stack">
                        <WelcomePanel />
                        {notices.length
                            ? <div className="welcome-notices">{notices.map((notice) => <ModelSwitchNotice key={notice.id} notice={notice} />)}</div>
                            : null}
                    </div>}
                <button
                    className="scroll-to-latest"
                    data-visible={latestVisible}
                    aria-label={t('scrollToLatest')}
                    aria-hidden={!latestVisible}
                    disabled={!latestVisible}
                    tabIndex={latestVisible ? 0 : -1}
                    onClick={scrollToLatest}
                    type="button"
                >
                    {pendingPrompt ? <span className="latest-output-dots" aria-hidden="true">
                        <span className="latest-output-dot" />
                        <span className="latest-output-dot" />
                        <span className="latest-output-dot" />
                    </span> : <svg aria-hidden="true" width="16" height="16" viewBox="0 0 20 20" fill="none">
                        <path d="M10 3v14m-6-6 6 6 6-6" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
                    </svg>}
                </button>
            </div>
            <div className="composer-slot"><Composer /></div>
        </Container>
    );
};
