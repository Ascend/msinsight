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
import type { TFunction } from 'i18next';
import React, { useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import type { CSSProperties, RefObject } from 'react';
import { useTranslation } from 'react-i18next';
import { formatRelativeTime } from '@insight/lib/utils';
import type { SessionItem } from '../types';
import deleteIcon from '../icons/delete.svg';
import searchIcon from '../icons/search.svg';

interface SessionHistoryPopoverProps {
    anchorRef: RefObject<HTMLElement>;
    currentSessionId?: string;
    onClose: () => void;
    onDelete: (session: SessionItem) => void;
    onAttentionChange?: (attentionRequired: boolean) => void;
    onSelect: (session: SessionItem) => void;
    open: boolean;
    sessions: SessionItem[];
}

interface PopoverPosition {
    left: number;
    top: number;
}

const POPOVER_WIDTH = 300;
const POPOVER_MAX_HEIGHT = 500;
const POPOVER_GAP = 8;
const VIEWPORT_PADDING = 8;

const PopoverContainer = styled.div`
    position: fixed;
    z-index: 1200;
    width: ${POPOVER_WIDTH}px;
    max-height: ${POPOVER_MAX_HEIGHT}px;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 12px;
    padding: 8px;
    background: ${(props): string => props.theme.mode === 'dark' ? props.theme.bgColorLight : props.theme.bgColor};
    box-shadow: ${(props): string => props.theme.mode === 'dark' ? props.theme.boxShadow : props.theme.boxShadowDark};
    color: ${(props): string => props.theme.textColorPrimary};

    .history-header {
        flex: 0 0 auto;
        padding: 0 0 8px;
    }

    .history-title {
        margin: 2px 4px 8px;
        color: ${(props): string => props.theme.mode === 'dark'
        ? props.theme.textColorTertiary
        : props.theme.textColorDisabled};
        font-size: 12px;
        line-height: 18px;
    }

    .history-search {
        height: 32px;
        display: flex;
        align-items: center;
        gap: 10px;
        padding: 0 12px;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        background: transparent;
    }

    .history-search:focus-within {
        border-color: ${(props): string => props.theme.primaryColor};
        box-shadow: 0 0 0 2px ${(props): string => props.theme.primaryColorLight5};
    }

    .search-icon {
        width: 16px;
        height: 16px;
        flex: 0 0 16px;
    }

    .history-search input {
        width: 100%;
        min-width: 0;
        border: 0;
        outline: 0;
        padding: 0;
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 14px;
    }

    .history-search input::placeholder {
        color: ${(props): string => props.theme.mode === 'dark'
        ? props.theme.textColorTertiary
        : props.theme.textColorDisabled};
    }

    .history-list {
        min-height: 0;
        display: grid;
        gap: 2px;
        overflow-y: auto;
        scrollbar-gutter: stable;
    }

    .history-row {
        position: relative;
        min-width: 0;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
    }

    .history-row:hover {
        background: ${(props): string => props.theme.mode === 'dark'
        ? props.theme.agentWelcomeCardBackgroundColor
        : props.theme.bgColorLight};
    }

    .history-row.active {
        background: ${(props): string => props.theme.primaryColorLight5};
    }

    .history-session {
        width: 100%;
        min-width: 0;
        height: 30px;
        display: grid;
        grid-template-columns: 14px minmax(0, 1fr) auto;
        align-items: center;
        gap: 8px;
        border: 0;
        padding: 0 8px;
        background: transparent;
        color: inherit;
        text-align: left;
        cursor: pointer;
    }

    .history-session:disabled {
        color: ${(props): string => props.theme.mode === 'dark'
        ? props.theme.textColorTertiary
        : props.theme.textColorDisabled};
        cursor: not-allowed;
    }

    .session-state {
        width: 8px;
        height: 8px;
        border-radius: 50%;
        background: ${(props): string => props.theme.borderColor};
    }

    .history-row.unread .session-state {
        background: ${(props): string => props.theme.primaryColor};
    }

    .history-row.working .session-state {
        width: 12px;
        height: 12px;
        border: 0;
        background: conic-gradient(
            ${(props): string => props.theme.primaryColor} 0 60%,
            ${(props): string => props.theme.borderColor} 60% 100%
        );
        mask: radial-gradient(farthest-side, transparent calc(100% - 2px), #000 0);
        -webkit-mask: radial-gradient(farthest-side, transparent calc(100% - 2px), #000 0);
    }

    .history-row.error .session-state {
        background: ${(props): string => props.theme.dangerColor};
    }

    .session-title {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        font-size: 14px;
        line-height: 20px;
    }

    .session-meta {
        justify-self: end;
        text-align: right;
        color: ${(props): string => props.theme.textColorDisabled};
        font-size: 12px;
        line-height: 18px;
    }

    .history-row:hover .session-meta {
        visibility: hidden;
    }

    .session-delete {
        width: 24px;
        height: 24px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        position: absolute;
        top: 3px;
        right: 2px;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        padding: 4px;
        background: transparent;
        cursor: pointer;
        opacity: 0;
        pointer-events: none;
        transition: opacity 120ms ease;
    }

    .history-row:hover .session-delete,
    .session-delete:focus-visible {
        opacity: 1;
        pointer-events: auto;
    }

    .session-delete:hover {
        background: ${(props): string => props.theme.bgColorDark};
    }

    .session-delete img {
        width: 16px;
        height: 16px;
        opacity: 0.65;
        filter: ${(props): string => props.theme.mode === 'dark' ? 'invert(1)' : 'none'};
    }

    .history-empty {
        padding: 48px 12px;
        color: ${(props): string => props.theme.mode === 'dark'
        ? props.theme.textColorTertiary
        : props.theme.textColorDisabled};
        font-size: 13px;
        text-align: center;
    }
`;

export const SessionHistoryPopover = ({
    anchorRef,
    currentSessionId,
    onClose,
    onDelete,
    onAttentionChange,
    onSelect,
    open,
    sessions,
}: SessionHistoryPopoverProps): JSX.Element | null => {
    const { t } = useTranslation('insightWebAgent');
    const popoverRef = useRef<HTMLDivElement>(null);
    const searchRef = useRef<HTMLInputElement>(null);
    const previousStatusesRef = useRef<Map<string, string>>(new Map());
    const [query, setQuery] = useState('');
    const [position, setPosition] = useState<PopoverPosition | null>(null);
    const [unreadSessionIds, setUnreadSessionIds] = useState<Set<string>>(new Set());
    const filteredSessions = useMemo(() => {
        const normalizedQuery = query.trim().toLocaleLowerCase();
        if (!normalizedQuery) return sessions;
        return sessions.filter((session) => getSessionTitle(session).toLocaleLowerCase().includes(normalizedQuery));
    }, [query, sessions]);

    useEffect(() => {
        const nextStatuses = new Map<string, string>();
        const completedSessionIds: string[] = [];
        sessions.forEach((session) => {
            const status = getEffectiveStatus(session);
            const previousStatus = previousStatusesRef.current.get(session.sessionId);
            nextStatuses.set(session.sessionId, status);
            if (status === 'completed' && previousStatus !== undefined && previousStatus !== 'completed') {
                completedSessionIds.push(session.sessionId);
            }
        });
        previousStatusesRef.current = nextStatuses;
        setUnreadSessionIds((current) => {
            const next = new Set([...current].filter((sessionId) => nextStatuses.has(sessionId)));
            completedSessionIds.forEach((sessionId) => {
                if (sessionId !== currentSessionId) next.add(sessionId);
            });
            if (currentSessionId) next.delete(currentSessionId);
            return areStringSetsEqual(current, next) ? current : next;
        });
    }, [currentSessionId, sessions]);

    useEffect(() => {
        const hasUnfinishedSession = sessions.some((session) => {
            return session.isPending === true ||
                session.pendingPrompt === true ||
                session.status === 'loading' ||
                session.status === 'working';
        });
        onAttentionChange?.(hasUnfinishedSession || unreadSessionIds.size > 0);
    }, [onAttentionChange, sessions, unreadSessionIds]);

    useEffect(() => {
        if (!open) {
            setQuery('');
            setPosition(null);
            return;
        }
        const handlePointerDown = (event: PointerEvent): void => {
            const target = event.target as Node;
            if (!anchorRef.current?.contains(target) && !popoverRef.current?.contains(target)) onClose();
        };
        const handleKeyDown = (event: KeyboardEvent): void => {
            if (event.key === 'Escape') onClose();
        };
        document.addEventListener('pointerdown', handlePointerDown);
        document.addEventListener('keydown', handleKeyDown);
        requestAnimationFrame(() => searchRef.current?.focus());
        return () => {
            document.removeEventListener('pointerdown', handlePointerDown);
            document.removeEventListener('keydown', handleKeyDown);
        };
    }, [anchorRef, onClose, open]);

    useLayoutEffect(() => {
        if (!open) return;
        const updatePosition = (): void => {
            const anchor = anchorRef.current;
            if (!anchor) return;
            const anchorRect = anchor.getBoundingClientRect();
            const popoverHeight = Math.min(popoverRef.current?.offsetHeight ?? POPOVER_MAX_HEIGHT, POPOVER_MAX_HEIGHT);
            const left = Math.min(
                Math.max(VIEWPORT_PADDING, anchorRect.right - POPOVER_WIDTH),
                Math.max(VIEWPORT_PADDING, window.innerWidth - POPOVER_WIDTH - VIEWPORT_PADDING),
            );
            const spaceBelow = window.innerHeight - anchorRect.bottom - POPOVER_GAP - VIEWPORT_PADDING;
            const top = spaceBelow >= popoverHeight
                ? anchorRect.bottom + POPOVER_GAP
                : Math.max(VIEWPORT_PADDING, anchorRect.top - POPOVER_GAP - popoverHeight);
            setPosition({ left, top });
        };
        updatePosition();
        window.addEventListener('resize', updatePosition);
        window.addEventListener('scroll', updatePosition, true);
        return () => {
            window.removeEventListener('resize', updatePosition);
            window.removeEventListener('scroll', updatePosition, true);
        };
    }, [anchorRef, filteredSessions.length, open]);

    if (!open) return null;
    const popoverStyle: CSSProperties = position
        ? { left: position.left, top: position.top }
        : { left: 0, top: 0, visibility: 'hidden' };
    const sessionList = filteredSessions.map((session) => {
        const active = session.sessionId === currentSessionId;
        const status = getEffectiveStatus(session);
        const unread = unreadSessionIds.has(session.sessionId);
        const sessionTitle = getSessionTitle(session);
        return (
            <div className={`history-row ${active ? 'active' : ''} ${unread ? 'unread' : ''} ${status}`} key={session.sessionId}>
                <button
                    aria-label={sessionTitle}
                    className="history-session"
                    disabled={session.isPending}
                    onClick={() => {
                        setUnreadSessionIds((current) => {
                            if (!current.has(session.sessionId)) return current;
                            const next = new Set(current);
                            next.delete(session.sessionId);
                            return next;
                        });
                        onSelect(session);
                        onClose();
                    }}
                    title={sessionTitle}
                    type="button"
                >
                    <span aria-hidden="true" className="session-state" />
                    <span className="session-title">{sessionTitle}</span>
                    <span className="session-meta">{getSessionMeta(session, t, unread)}</span>
                </button>
                <button
                    aria-label={t('deleteSession', { title: sessionTitle })}
                    className="session-delete"
                    onClick={() => onDelete(session)}
                    title={t('deleteSession', { title: sessionTitle })}
                    type="button"
                >
                    <img alt="" src={deleteIcon} />
                </button>
            </div>
        );
    });

    return createPortal(
        <PopoverContainer aria-label={t('sessionHistory')} ref={popoverRef} role="dialog" style={popoverStyle}>
            <div className="history-header">
                <div className="history-title">{t('sessionHistory')}</div>
                <label className="history-search">
                    <img alt="" className="search-icon" src={searchIcon} />
                    <input
                        aria-label={t('searchSessions')}
                        onChange={(event) => setQuery(event.target.value)}
                        placeholder={t('searchSessionsPlaceholder')}
                        ref={searchRef}
                        type="search"
                        value={query}
                    />
                </label>
            </div>
            <div className="history-list">
                {filteredSessions.length > 0
                    ? sessionList
                    : <div className="history-empty">{t('noMatchingSessions')}</div>}
            </div>
        </PopoverContainer>,
        document.body,
    );
};

const getSessionMeta = (session: SessionItem, t: TFunction, unread: boolean): string => {
    if (session.pendingPrompt === true || session.status === 'working') return t('working');
    if (session.status === 'completed' && unread) return t('completed');
    if (session.status === 'loading') return t('loading');
    if (session.status === 'error') return t('loadFailed');
    return session.updatedAt ? formatRelativeTime(session.updatedAt) : session.sessionId;
};

const getSessionTitle = (session: SessionItem): string => session.title?.trim() ? session.title : session.sessionId;

const getEffectiveStatus = (session: SessionItem): string => {
    return session.pendingPrompt === true || session.status === 'working' ? 'working' : session.status ?? '';
};

const areStringSetsEqual = (left: Set<string>, right: Set<string>): boolean => {
    return left.size === right.size && [...left].every((value) => right.has(value));
};
