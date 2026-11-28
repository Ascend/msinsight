/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 * -------------------------------------------------------------------------
 */

import styled from '@emotion/styled';
import { observer } from 'mobx-react-lite';
import React from 'react';
import type { Session } from '../entity/session';
import { deleteSliceMark, jumpToSliceMark } from '../utils/sliceMark';

interface DragPosition {
    left: number;
    top: number;
}

interface DragState {
    pointerId: number;
    startX: number;
    startY: number;
    origin: DragPosition;
    minDeltaX: number;
    maxDeltaX: number;
    minDeltaY: number;
    maxDeltaY: number;
}

const clamp = (value: number, min: number, max: number): number => Math.min(Math.max(value, min), max);

const MarkListContainer = styled.div<{ $position?: DragPosition }>`
    position: absolute;
    left: ${(props): string => props.$position === undefined ? 'auto' : `${props.$position.left}px`};
    top: ${(props): string => props.$position === undefined ? 'auto' : `${props.$position.top}px`};
    right: ${(props): string => props.$position === undefined ? '8px' : 'auto'};
    bottom: ${(props): string => props.$position === undefined ? '32px' : 'auto'};
    z-index: 11;
    display: flex;
    flex-direction: column;
    gap: 4px;
    max-width: 280px;
    padding: 6px;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 4px;
    background-color: ${(props): string => props.theme.contentBackgroundColor};
    box-shadow: ${(props): string => props.theme.boxShadow};
`;

const DragHandle = styled.button`
    display: flex;
    align-items: center;
    justify-content: center;
    width: 100%;
    height: 12px;
    padding: 0;
    border: none;
    color: ${(props): string => props.theme.iconColor};
    background: transparent;
    cursor: grab;
    touch-action: none;
    user-select: none;

    &::before {
        content: '...';
        letter-spacing: 3px;
        line-height: 6px;
    }

    &:active {
        cursor: grabbing;
    }
`;

const MarkItem = styled.div`
    display: flex;
    align-items: center;
    min-width: 120px;
    max-width: 260px;
    gap: 4px;
`;

const MarkJumpButton = styled.button`
    display: flex;
    flex: 1;
    align-items: center;
    min-width: 0;
    gap: 6px;
    padding: 2px 4px;
    overflow: hidden;
    border: none;
    border-radius: 3px;
    color: ${(props): string => props.theme.textColorPrimary};
    background: transparent;
    cursor: pointer;
    text-align: left;

    &:hover {
        background: ${(props): string => props.theme.bgColorLight};
    }
`;

const MarkKey = styled.kbd`
    display: inline-flex;
    flex: 0 0 18px;
    align-items: center;
    justify-content: center;
    height: 18px;
    border: 1px solid ${(props): string => props.theme.primaryColor};
    border-radius: 3px;
    color: ${(props): string => props.theme.primaryColor};
    font-weight: bold;
    line-height: 1;
`;

const MarkName = styled.span`
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
`;

const DeleteButton = styled.button`
    width: 20px;
    height: 20px;
    padding: 0;
    border: none;
    border-radius: 3px;
    color: ${(props): string => props.theme.iconColor};
    background: transparent;
    cursor: pointer;
    font-size: 16px;
    line-height: 18px;

    &:hover {
        color: ${(props): string => props.theme.primaryColor};
        background: ${(props): string => props.theme.bgColorLight};
    }
`;

export const SliceMarkList = observer(({ session }: { session: Session }): JSX.Element | null => {
    const [position, setPosition] = React.useState<DragPosition>();
    const dragState = React.useRef<DragState>();
    const panelRef = React.useRef<HTMLDivElement>(null);
    const marks = Array.from(session.sliceMarks.values()).sort((left, right) => Number(left.key) - Number(right.key));

    const clampToParent = React.useCallback((): void => {
        const panel = panelRef.current;
        const parent = panel?.parentElement;
        if (panel === null || parent === null || parent === undefined) {
            return;
        }
        const panelRect = panel.getBoundingClientRect();
        const parentRect = parent.getBoundingClientRect();
        const maxLeft = Math.max(0, parentRect.width - panelRect.width);
        const maxTop = Math.max(0, parentRect.height - panelRect.height);
        setPosition(current => {
            if (current === undefined) {
                return current;
            }
            const next = {
                left: clamp(current.left, 0, maxLeft),
                top: clamp(current.top, 0, maxTop),
            };
            return next.left === current.left && next.top === current.top ? current : next;
        });
    }, []);

    React.useLayoutEffect(() => {
        clampToParent();
    }, [marks.length, clampToParent]);

    React.useLayoutEffect(() => {
        const panel = panelRef.current;
        const parent = panel?.parentElement;
        if (panel === null || parent === null || parent === undefined || typeof ResizeObserver === 'undefined') {
            return;
        }
        const observer = new ResizeObserver((): void => {
            clampToParent();
        });
        observer.observe(parent);
        observer.observe(panel);
        return (): void => observer.disconnect();
    }, [marks.length, clampToParent]);

    const handlePointerDown = (event: React.PointerEvent<HTMLButtonElement>): void => {
        if (event.button !== 0) {
            return;
        }
        const panel = event.currentTarget.parentElement;
        const parent = panel?.parentElement;
        if (panel === null || parent === null || panel === undefined || parent === undefined) {
            return;
        }
        const panelRect = panel.getBoundingClientRect();
        const parentRect = parent.getBoundingClientRect();
        dragState.current = {
            pointerId: event.pointerId,
            startX: event.clientX,
            startY: event.clientY,
            origin: {
                left: panelRect.left - parentRect.left,
                top: panelRect.top - parentRect.top,
            },
            minDeltaX: parentRect.left - panelRect.left,
            maxDeltaX: parentRect.right - panelRect.right,
            minDeltaY: parentRect.top - panelRect.top,
            maxDeltaY: parentRect.bottom - panelRect.bottom,
        };
        event.currentTarget.setPointerCapture?.(event.pointerId);
        event.preventDefault();
        event.stopPropagation();
    };

    const handlePointerMove = (event: React.PointerEvent<HTMLButtonElement>): void => {
        const drag = dragState.current;
        if (drag === undefined || drag.pointerId !== event.pointerId) {
            return;
        }
        const deltaX = clamp(event.clientX - drag.startX, drag.minDeltaX, drag.maxDeltaX);
        const deltaY = clamp(event.clientY - drag.startY, drag.minDeltaY, drag.maxDeltaY);
        setPosition({ left: drag.origin.left + deltaX, top: drag.origin.top + deltaY });
        event.preventDefault();
        event.stopPropagation();
    };

    const handlePointerEnd = (event: React.PointerEvent<HTMLButtonElement>): void => {
        if (dragState.current?.pointerId !== event.pointerId) {
            return;
        }
        dragState.current = undefined;
        event.currentTarget.releasePointerCapture?.(event.pointerId);
        event.stopPropagation();
    };

    if (marks.length === 0) {
        return null;
    }
    return <MarkListContainer ref={panelRef} $position={position} data-testid="timeline-slice-mark-list"
        onMouseDown={(event): void => event.stopPropagation()}>
        <DragHandle
            type="button"
            aria-label="Move slice marks"
            title="Move slice marks"
            data-testid="timeline-slice-mark-drag-handle"
            onPointerDown={handlePointerDown}
            onPointerMove={handlePointerMove}
            onPointerUp={handlePointerEnd}
            onPointerCancel={handlePointerEnd}
        />
        {marks.map((mark) => <MarkItem key={mark.key}>
            <MarkJumpButton
                type="button"
                title={mark.name}
                data-testid={`timeline-slice-mark-${mark.key}`}
                onClick={() => jumpToSliceMark(session, mark.key)}
            >
                <MarkKey>{mark.key}</MarkKey>
                <MarkName>{mark.name}</MarkName>
            </MarkJumpButton>
            <DeleteButton
                type="button"
                aria-label={`Delete mark ${mark.key}`}
                data-testid={`timeline-slice-mark-delete-${mark.key}`}
                onClick={(event) => {
                    event.stopPropagation();
                    deleteSliceMark(session, mark.key);
                }}
            >
                x
            </DeleteButton>
        </MarkItem>)}
    </MarkListContainer>;
});
