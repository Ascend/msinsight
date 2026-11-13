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
import React, { useState } from 'react';
import styled from '@emotion/styled';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import type { MemoryContext } from '../common/shared/state/MemoryContext';
import { TimelineFlagIcon } from '../common/components/leaks/TimelineFlagIcon';
import { eventCoordinateToX, screenXToEventCoordinate } from './timeline';

export const SNAPSHOT_EVENT_AXIS_OFFSET = 38;
const AXIS_HEIGHT = 30;

const MarkerAxis = styled.div`
    position: absolute;
    top: -${SNAPSHOT_EVENT_AXIS_OFFSET}px;
    left: 0;
    width: 100%;
    height: ${AXIS_HEIGHT}px;
    box-sizing: border-box;
    z-index: 4;
    background: ${(p): string => p.theme.bgColorLight};
    border: 1px solid ${(p): string => p.theme.borderColorLight};
`;
const CreateSurface = styled.button`
    position: absolute;
    inset: 0;
    width: 100%;
    padding: 0;
    background: transparent;
    border: 0;
    cursor: default;
    touch-action: none;
    &:hover { background: ${(p): string => p.theme.bgColorCommon}; box-shadow: inset 0 -2px 0 ${(p): string => p.theme.borderColor}; }
    &:focus-visible { outline: 2px solid ${(p): string => p.theme.primaryColor}; outline-offset: -2px; }
`;
const AxisRail = styled.div`
    position: absolute;
    top: 0;
    left: 4px;
    right: 4px;
    height: 8px;
    background: ${(p): string => p.theme.bgColorCommon};
    border-bottom: 1px solid ${(p): string => p.theme.borderColor};
    opacity: .68;
    pointer-events: none;
`;
const AxisFlag = styled.span<{ $preview: boolean }>`
    position: absolute;
    top: 4px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 22px;
    height: 22px;
    box-sizing: border-box;
    color: ${(p): string => p.theme.primaryColor};
    background: ${(p): string => p.theme.bgColorCommon};
    border: 1px ${(p): string => p.$preview ? 'dashed' : 'solid'} currentColor;
    border-radius: 3px;
    opacity: ${(p): number => p.$preview ? 0.65 : 1};
    transform: translateX(-50%);
    pointer-events: none;
    svg { width: 19px; height: auto; }
`;

const Flag = styled.div`
    position: absolute;
    top: 0;
    width: 0;
    z-index: 4;
    color: ${(p): string => p.theme.primaryColor};
    border-left: 1px dashed currentColor;
    pointer-events: none;
    span { position: absolute; top: 0; left: -4px; pointer-events: auto; background: ${(p): string => p.theme.bgColorCommon}; border-radius: 2px; }
`;
export const SnapshotEventFlag = observer(({ session, eventId, onSelect }: {
    session: MemoryContext;
    eventId?: number;
    onSelect?: (coordinate: number) => void;
}): JSX.Element | null => {
    const { t } = useTranslation('leaks');
    const [preview, setPreview] = useState<{ key: string; x: number } | null>(null);
    const { sizeInfo, renderOptions: { transform, viewport, zoom } } = session.leaksWorkerInfo;
    const projection = {
        minEventId: sizeInfo.minTimestamp,
        maxEventId: sizeInfo.maxTimestamp,
        width: viewport.width,
        scaleX: transform.scaleX,
        transformX: transform.x,
        zoomX: zoom.x,
    };
    const projectionKey = JSON.stringify([session.deviceId, ...Object.values(projection)]);
    const x = eventId === undefined ? null : eventCoordinateToX(eventId, projection);
    const effectivePreview = onSelect !== undefined && preview?.key === projectionKey && (x === null || Math.abs(preview.x - x) > 0.5) ? preview : null;
    const hint = t('frameTree.selectEventFromAxis');
    const label = eventId === undefined ? hint : `${t('frameTree.device')} ${session.deviceId} \u00b7 ${t('frameTree.event')} ${eventId}`;
    const projectPointer = (surface: HTMLButtonElement, clientX: number): { coordinate: number; x: number } | null => {
        const rect = surface.getBoundingClientRect();
        if (rect.width <= 0) return null;
        const screenX = Math.min(Math.max(clientX - rect.left, 0), rect.width) * viewport.width / rect.width;
        const coordinate = screenXToEventCoordinate(screenX, projection);
        return coordinate === null ? null : { coordinate, x: screenX };
    };
    if (onSelect === undefined && x === null) return null;
    return <>
        {onSelect === undefined
            ? null
            : <MarkerAxis data-testid="snapshot-event-marker-axis">
                <CreateSurface type="button" aria-label={hint} title={eventId === undefined ? hint : `${label}\n${hint}`}
                    onMouseMove={(event): void => {
                        const candidate = projectPointer(event.currentTarget, event.clientX);
                        setPreview(candidate === null ? null : { key: projectionKey, x: candidate.x });
                    }}
                    onMouseLeave={(): void => setPreview(null)}
                    onClick={(event): void => {
                        event.stopPropagation();
                        const rect = event.currentTarget.getBoundingClientRect();
                        const candidate = projectPointer(event.currentTarget, event.detail === 0 ? rect.left + rect.width / 2 : event.clientX);
                        setPreview(null);
                        if (candidate !== null) onSelect(candidate.coordinate);
                    }} />
                <AxisRail />
                {x === null ? null : <AxisFlag $preview={false} style={{ left: x }} title={label} aria-hidden="true"><TimelineFlagIcon /></AxisFlag>}
                {effectivePreview === null || x !== null ? null : <AxisFlag $preview style={{ left: effectivePreview.x }} aria-hidden="true"><TimelineFlagIcon /></AxisFlag>}
            </MarkerAxis>}
        {x === null
            ? null
            : <Flag data-testid="snapshot-event-flag" style={{ left: x, height: viewport.height }} aria-label={label}>
                {onSelect === undefined ? <span title={label}><TimelineFlagIcon /></span> : null}
            </Flag>}
        {effectivePreview === null ? null : <Flag data-testid="snapshot-event-preview" style={{ left: effectivePreview.x, height: viewport.height, opacity: 0.45 }} aria-hidden="true" />}
    </>;
});
