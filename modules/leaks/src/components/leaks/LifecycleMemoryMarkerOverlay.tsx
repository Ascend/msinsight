/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React, { useMemo, useState } from 'react';
import styled from '@emotion/styled';
import { DeleteOutlined } from '@ant-design/icons';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import { type Session } from '../../entity/session';
import { formatBytes } from '../../utils/utils';
import {
    DEFAULT_LIFECYCLE_MEMORY_MARKER_COLOR,
    getLifecycleMemoryMarkerColor,
    getLifecycleMemoryMarkerOrdinal,
    sortLifecycleMemoryMarkers,
} from '../../entity/lifecycleMemoryMarkers';
import { memoryValueToScreenY, screenYToMemoryValue } from './lifecycleNavigation';
import { TimelineFlagIcon } from './TimelineFlagIcon';

const MARKER_AXIS_RIGHT = -38;
const MARKER_AXIS_WIDTH = 30;
const FLAG_EDGE_MARGIN = 10;
const AXIS_RAIL_WIDTH = 8;
const FLAG_LANE_LEFT = 7;

const Overlay = styled.div`
    position: absolute;
    inset: 0;
    z-index: 3;
    overflow: visible;
    pointer-events: none;
`;

const MarkerLine = styled.div<{ markerColor: string }>`
    position: absolute;
    left: 0;
    width: 100%;
    border-top: 1px dashed ${(props): string => props.markerColor};
    opacity: 0.55;
    pointer-events: none;
`;

const AxisPreviewLine = styled.div<{ previewColor: string }>`
    position: absolute;
    left: 0;
    width: 100%;
    border-top: 1px dashed ${(props): string => props.previewColor};
    opacity: 0.55;
    pointer-events: none;
`;

const AxisPreviewValue = styled.div`
    position: absolute;
    right: 4px;
    padding: 1px 5px;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 11px;
    font-variant-numeric: tabular-nums;
    line-height: 16px;
    white-space: nowrap;
    background: ${(props): string => props.theme.bgColorCommon};
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 3px;
    transform: translateY(calc(-100% - 4px));
    pointer-events: none;
`;

const MarkerAxis = styled.div`
    position: absolute;
    top: 0;
    right: ${MARKER_AXIS_RIGHT}px;
    width: ${MARKER_AXIS_WIDTH}px;
    height: 100%;
    box-sizing: border-box;
    background: ${(props): string => props.theme.bgColorLight};
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    pointer-events: auto;
`;

const AxisCreateSurface = styled.button`
    position: absolute;
    inset: 0;
    width: 100%;
    padding: 0;
    background: transparent;
    border: 0;
    cursor: default;

    &:hover {
        background: ${(props): string => props.theme.bgColorCommon};
        box-shadow: inset 2px 0 0 ${(props): string => props.theme.borderColor};
    }

    &:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: -2px;
    }
`;

const AxisPreviewFlag = styled.div`
    position: absolute;
    left: ${FLAG_LANE_LEFT}px;
    z-index: 4;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 22px;
    height: 22px;
    box-sizing: border-box;
    color: inherit;
    background: ${(props): string => props.theme.bgColorCommon};
    border: 1px dashed currentColor;
    border-radius: 3px;
    opacity: 0.82;
    transform: translateY(-50%);
    pointer-events: none;

    svg {
        width: 18px;
        height: auto;
    }
`;

const AxisRail = styled.div`
    position: absolute;
    top: 4px;
    bottom: 4px;
    left: 0;
    width: ${AXIS_RAIL_WIDTH}px;
    background: ${(props): string => props.theme.bgColorCommon};
    border-right: 1px solid ${(props): string => props.theme.borderColor};
    opacity: 0.68;
    pointer-events: none;
`;

const AxisTick = styled.div<{ markerColor: string }>`
    position: absolute;
    left: 0;
    width: ${AXIS_RAIL_WIDTH}px;
    border-top: 1px solid ${(props): string => props.markerColor};
    opacity: 0.6;
    pointer-events: none;
`;

const FlagPosition = styled.div`
    position: absolute;
    left: ${FLAG_LANE_LEFT}px;
    z-index: 2;
    transform: translateY(-50%);
    pointer-events: auto;

    &:hover,
    &:focus-within {
        z-index: 8;
    }

    &:hover [data-marker-delete],
    &:focus-within [data-marker-delete] {
        opacity: 1;
        visibility: visible;
    }
`;

const MarkerFlag = styled.button<{ markerColor: string }>`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 22px;
    height: 22px;
    padding: 0;
    box-sizing: border-box;
    color: ${(props): string => props.markerColor};
    background: transparent;
    border: 1px solid transparent;
    border-radius: 3px;
    cursor: default;
    touch-action: none;
    user-select: none;

    svg {
        width: 19px;
        height: auto;
    }

    &:hover,
    &:focus-visible {
        background: ${(props): string => props.theme.bgColorCommon};
        border-color: ${(props): string => props.markerColor};
    }

    &:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        border-radius: 3px;
    }
`;

const DeleteBadge = styled.button`
    position: absolute;
    top: -7px;
    right: -6px;
    z-index: 3;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 16px;
    height: 16px;
    padding: 0;
    color: #fff;
    background: #e64545;
    border: 1px solid ${(props): string => props.theme.bgColorCommon};
    border-radius: 50%;
    cursor: pointer;
    opacity: 0;
    visibility: hidden;
    transition: opacity 160ms ease-out;

    &:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 1px;
        opacity: 1;
        visibility: visible;
    }

    svg {
        width: 10px;
        height: 10px;
    }
`;

export const LifecycleMemoryMarkerOverlay = observer(({
    session,
    onCreateMarker,
}: {
    session: Session;
    onCreateMarker: (memoryBytes: number) => void;
}): JSX.Element => {
    const { t } = useTranslation('leaks');
    const [axisPreview, setAxisPreview] = useState<{
        memoryBytes: number;
        screenY: number;
        color: string;
    } | null>(null);
    const { sizeInfo, renderOptions: { transform, zoom, viewport } } = session.leaksWorkerInfo;
    const markers = sortLifecycleMemoryMarkers(session.getLifecycleMemoryMarkers());
    const projectionOptions = {
        minSize: sizeInfo.minSize,
        maxSize: sizeInfo.maxSize,
        transformY: transform.y,
        scaleY: transform.scaleY,
        zoomY: zoom.y,
        viewportHeight: viewport.height,
    };
    const projectedMarkers = useMemo(() => markers.reduce<Array<typeof markers[number] & {
        screenY: number;
        ordinal: number;
        color: string;
    }>>((result, marker, index) => {
        const screenY = memoryValueToScreenY(marker.memoryBytes, projectionOptions);
        if (screenY !== null && screenY >= 0 && screenY <= viewport.height) {
            result.push({
                ...marker,
                screenY,
                ordinal: getLifecycleMemoryMarkerOrdinal(marker, index),
                color: getLifecycleMemoryMarkerColor(marker, index),
            });
        }
        return result;
    }, []), [markers, sizeInfo, transform, zoom, viewport]);
    const flagEdge = Math.min(FLAG_EDGE_MARGIN, viewport.height / 2);
    const flagLayout = projectedMarkers.map(marker => ({
        id: marker.id,
        actualY: marker.screenY,
        displayY: Math.min(Math.max(marker.screenY, flagEdge), Math.max(flagEdge, viewport.height - flagEdge)),
    }));
    const layoutById = new Map(flagLayout.map(item => [item.id, item]));

    const projectAxisPointer = (surface: HTMLElement, clientY: number): typeof axisPreview => {
        const rect = surface.getBoundingClientRect();
        const localY = Math.min(Math.max(clientY - rect.top, 0), rect.height);
        const screenY = rect.height > 0 ? localY * viewport.height / rect.height : localY;
        const memoryBytes = screenYToMemoryValue(screenY, projectionOptions);
        return memoryBytes === null
            ? null
            : {
                memoryBytes: Math.min(Math.max(memoryBytes, sizeInfo.minSize), sizeInfo.maxSize),
                screenY,
                color: DEFAULT_LIFECYCLE_MEMORY_MARKER_COLOR,
            };
    };

    const createMarkerFromAxis = (event: React.MouseEvent<HTMLButtonElement>): void => {
        event.stopPropagation();
        const rect = event.currentTarget.getBoundingClientRect();
        const preview = event.detail === 0
            ? projectAxisPointer(event.currentTarget, rect.top + rect.height / 2)
            : projectAxisPointer(event.currentTarget, event.clientY);
        if (preview !== null) {
            onCreateMarker(preview.memoryBytes);
        }
    };

    return <Overlay data-testid="lifecycleMemoryMarkerOverlay">
        {projectedMarkers.map(marker => <MarkerLine
            key={marker.id}
            markerColor={marker.color}
            style={{ top: marker.screenY }}
            data-marker-id={marker.id}
        />)}
        {axisPreview === null
            ? <></>
            : <>
                <AxisPreviewLine
                    data-testid="memoryMarkerAxisPreviewLine"
                    previewColor={axisPreview.color}
                    style={{ top: axisPreview.screenY }}
                />
                <AxisPreviewValue style={{ top: axisPreview.screenY }}>
                    {formatBytes(axisPreview.memoryBytes)}
                </AxisPreviewValue>
            </>}
        <MarkerAxis data-testid="lifecycleMemoryMarkerAxis">
            <AxisCreateSurface
                type="button"
                aria-label={t('addMemoryMarkerFromAxis')}
                onClick={createMarkerFromAxis}
                onMouseMove={(event): void => setAxisPreview(projectAxisPointer(event.currentTarget, event.clientY))}
                onMouseLeave={(): void => setAxisPreview(null)}
            />
            {axisPreview === null
                ? <></>
                : <AxisPreviewFlag
                    data-testid="memoryMarkerAxisPreviewFlag"
                    style={{ top: axisPreview.screenY, color: axisPreview.color }}
                ><TimelineFlagIcon aria-hidden="true" /></AxisPreviewFlag>}
            <AxisRail />
            {projectedMarkers.map(marker => {
                const layout = layoutById.get(marker.id);
                if (layout === undefined) {
                    return <React.Fragment key={marker.id} />;
                }
                const markerLabel = `Flag ${marker.ordinal}`;
                return <React.Fragment key={marker.id}>
                    <AxisTick
                        markerColor={marker.color}
                        style={{ top: layout.actualY }}
                        data-testid="memoryMarkerBaselineGuide"
                        data-marker-id={marker.id}
                    />
                    <FlagPosition style={{ top: layout.displayY }} data-marker-ordinal={marker.ordinal}>
                        <MarkerFlag
                            type="button"
                            markerColor={marker.color}
                            aria-label={`${markerLabel}, ${t('memoryMarkerBaseline')}: ${formatBytes(marker.memoryBytes)}`}
                            onClick={(event): void => event.stopPropagation()}
                        ><TimelineFlagIcon aria-hidden="true" /></MarkerFlag>
                        <DeleteBadge
                            type="button"
                            data-marker-delete="true"
                            aria-label={`${t('deleteMemoryMarker')}: ${markerLabel}`}
                            onClick={(event): void => {
                                event.stopPropagation();
                                session.deleteLifecycleMemoryMarker(marker.id);
                            }}
                        ><DeleteOutlined /></DeleteBadge>
                    </FlagPosition>
                </React.Fragment>;
            })}
        </MarkerAxis>
    </Overlay>;
});
