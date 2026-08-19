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
    getLifecycleMemoryMarkerLabel,
    getLifecycleMemoryMarkerOrdinal,
    getLifecycleMemoryMarkerSource,
    sortLifecycleMemoryMarkers,
    type LifecycleMemoryMarker,
    type LifecycleMemoryMarkerSource,
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

const MarkerLine = styled.div<{ markerColor: string; $emphasized: boolean }>`
    position: absolute;
    left: 0;
    width: 100%;
    border-top: ${(props): number => props.$emphasized ? 2 : 1}px dashed ${(props): string => props.markerColor};
    opacity: ${(props): number => props.$emphasized ? 0.9 : 0.55};
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

const AxisTick = styled.div<{ markerColor: string; $emphasized: boolean }>`
    position: absolute;
    left: 0;
    width: ${AXIS_RAIL_WIDTH}px;
    border-top: ${(props): number => props.$emphasized ? 2 : 1}px solid ${(props): string => props.markerColor};
    opacity: ${(props): number => props.$emphasized ? 0.95 : 0.6};
    pointer-events: none;
`;

const AxisConnector = styled.div<{ markerColor: string }>`
    position: absolute;
    left: ${AXIS_RAIL_WIDTH - 1}px;
    width: 1px;
    min-height: 1px;
    background: ${(props): string => props.markerColor};
    opacity: 0.55;
    pointer-events: none;
`;

const AxisGapSegment = styled.button<{ $active: boolean }>`
    position: absolute;
    left: 0;
    z-index: 1;
    width: ${AXIS_RAIL_WIDTH}px;
    min-height: 1px;
    padding: 0;
    background: transparent;
    border: 0;
    cursor: default;

    &::before {
        position: absolute;
        inset: 0;
        background: ${(props): string => props.theme.primaryColor};
        opacity: ${(props): number => props.$active ? 0.22 : 0};
        content: '';
        transition: opacity 160ms ease-out;
    }

    &:hover::before,
    &:focus-visible::before {
        opacity: 0.22;
    }

    &:focus-visible {
        outline: none;
    }
`;

const InlineGapValue = styled.span<{ $active: boolean }>`
    position: absolute;
    top: 50%;
    right: calc(100% + 5px);
    padding: 0 5px;
    color: ${(props): string => props.$active ? props.theme.primaryColor : props.theme.textColorSecondary};
    font-size: 10px;
    font-variant-numeric: tabular-nums;
    font-weight: ${(props): number => props.$active ? 600 : 400};
    line-height: 16px;
    white-space: nowrap;
    background: ${(props): string => props.theme.bgColorCommon};
    border: 1px solid ${(props): string => props.$active ? props.theme.borderColor : 'transparent'};
    border-radius: 8px;
    transform: translateY(-50%);
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

    &:hover [data-marker-details],
    &:focus-within [data-marker-details] {
        display: block;
    }
`;

const MarkerFlag = styled.button<{ markerColor: string; $emphasized: boolean }>`
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

    ${(props): string => props.$emphasized
        ? `
            background: ${props.theme.bgColorCommon};
            border-color: ${props.markerColor};
        `
        : ''}

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

type HoverCardPlacement = 'top' | 'center' | 'bottom';

const MarkerHoverCard = styled.div<{ placement: HoverCardPlacement }>`
    position: absolute;
    right: calc(100% + 8px);
    display: none;
    width: 224px;
    padding: 9px 10px;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 12px;
    font-variant-numeric: tabular-nums;
    line-height: 18px;
    background: ${(props): string => props.theme.bgColorCommon};
    border: 1px solid ${(props): string => props.theme.borderColorLight};
    border-radius: 5px;
    box-shadow: ${(props): string => props.theme.boxShadow};
    pointer-events: none;
    ${(props): string => props.placement === 'top'
        ? 'top: -4px;'
        : props.placement === 'bottom'
            ? 'bottom: -4px;'
            : 'top: 50%; transform: translateY(-50%);'}
`;

const RelationNode = styled.div`
    display: grid;
    grid-template-columns: 12px 48px minmax(0, 1fr);
    gap: 6px;
    align-items: center;
    min-height: 20px;
    color: ${(props): string => props.theme.textColorSecondary};
`;

const RelationFlag = styled.span<{ markerColor: string }>`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 12px;
    height: 16px;
    color: ${(props): string => props.markerColor};

    svg {
        width: 12px;
        height: auto;
    }
`;

const RelationMarkerIdentity = styled.span`
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 11px;
    font-weight: 500;
    white-space: nowrap;
`;

const RelationBaselineValue = styled.span`
    justify-self: end;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 11px;
    font-weight: 400;
    opacity: 0.72;
    white-space: nowrap;
`;

const RelationGap = styled.div`
    display: grid;
    grid-template-columns: 12px minmax(0, 1fr);
    gap: 5px;
    align-items: stretch;
    min-height: 25px;
`;

const RelationStem = styled.span`
    width: 1px;
    height: 100%;
    margin-left: 4px;
    background: ${(props): string => props.theme.borderColor};
`;

const RelationGapValue = styled.span`
    align-self: center;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 13px;
    font-weight: 600;
    line-height: 20px;
    white-space: nowrap;
`;

const NoNeighbor = styled.div`
    padding: 4px 0 1px 17px;
    color: ${(props): string => props.theme.textColorSecondary};
`;

const getHoverCardPlacement = (displayY: number, viewportHeight: number): HoverCardPlacement => {
    if (displayY < 76) return 'top';
    if (displayY > viewportHeight - 76) return 'bottom';
    return 'center';
};

export const LifecycleMemoryMarkerOverlay = observer(({
    session,
    onCreateMarker,
    onMarkerHoverChange = (): void => undefined,
    onGapHoverChange = (): void => undefined,
    blockPreview = null,
}: {
    session: Session;
    onCreateMarker: (memoryBytes: number, source: LifecycleMemoryMarkerSource) => void;
    onMarkerHoverChange?: (marker: LifecycleMemoryMarker | null) => void;
    onGapHoverChange?: (active: boolean, blockIds: number[]) => void;
    blockPreview?: { memoryBytes: number; color: string } | null;
}): JSX.Element => {
    const { t } = useTranslation('leaks');
    const [axisPreview, setAxisPreview] = useState<{
        memoryBytes: number;
        screenY: number;
        color: string;
        source: LifecycleMemoryMarkerSource;
    } | null>(null);
    const [emphasizedMarkerId, setEmphasizedMarkerId] = useState<string | null>(null);
    const [activeGapMarkerIds, setActiveGapMarkerIds] = useState<[string, string] | null>(null);
    const { sizeInfo, renderOptions: { transform, zoom, viewport } } = session.leaksWorkerInfo;
    const markers = sortLifecycleMemoryMarkers(
        session.getLifecycleMemoryMarkers().filter(marker => marker.hidden !== true),
    );
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
        source: LifecycleMemoryMarkerSource;
    }>>((result, marker, index) => {
        const screenY = memoryValueToScreenY(marker.memoryBytes, projectionOptions);
        if (screenY !== null && screenY >= 0 && screenY <= viewport.height) {
            result.push({
                ...marker,
                screenY,
                ordinal: getLifecycleMemoryMarkerOrdinal(marker, index),
                color: getLifecycleMemoryMarkerColor(marker, index),
                source: getLifecycleMemoryMarkerSource(marker),
            });
        }
        return result;
    }, []), [markers, sizeInfo, transform, zoom, viewport]);
    const flagEdge = Math.min(FLAG_EDGE_MARGIN, viewport.height / 2);
    const flagLayout = [...projectedMarkers]
        .sort((left, right) => left.screenY - right.screenY || left.id.localeCompare(right.id))
        .map(marker => ({
            id: marker.id,
            actualY: marker.screenY,
            displayY: Math.min(Math.max(marker.screenY, flagEdge), Math.max(flagEdge, viewport.height - flagEdge)),
        }));
    const layoutById = new Map(flagLayout.map(item => [item.id, item]));
    const markerById = new Map(projectedMarkers.map(marker => [marker.id, marker]));
    const markerIndexById = new Map(markers.map((marker, index) => [marker.id, index]));

    const getSpacing = (index: number, direction: 'upper' | 'lower'): number | null => {
        if (direction === 'lower') {
            return index > 0 ? markers[index].memoryBytes - markers[index - 1].memoryBytes : null;
        }
        return index < markers.length - 1 ? markers[index + 1].memoryBytes - markers[index].memoryBytes : null;
    };
    const formatSpacing = (value: number | null): string => value === null ? t('notAvailable') : formatBytes(value);

    const handleMarkerHoverChange = (marker: LifecycleMemoryMarker | null): void => {
        setEmphasizedMarkerId(marker?.id ?? null);
        onMarkerHoverChange(marker);
    };

    const handleGapHoverChange = (markerIds: [string, string] | null, blockIds: number[] = []): void => {
        setActiveGapMarkerIds(markerIds);
        onGapHoverChange(markerIds !== null, blockIds);
    };

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
                source: 'custom',
            };
    };

    const projectedBlockPreview = useMemo<typeof axisPreview>(() => {
        if (blockPreview === null) {
            return null;
        }
        const screenY = memoryValueToScreenY(blockPreview.memoryBytes, projectionOptions);
        if (screenY === null || screenY < 0 || screenY > viewport.height) {
            return null;
        }
        return {
            memoryBytes: blockPreview.memoryBytes,
            screenY,
            color: blockPreview.color,
            source: 'block',
        };
    }, [blockPreview, sizeInfo, transform, zoom, viewport]);
    const effectivePreview = axisPreview ?? projectedBlockPreview;

    const createMarkerFromAxis = (event: React.MouseEvent<HTMLButtonElement>): void => {
        event.stopPropagation();
        const rect = event.currentTarget.getBoundingClientRect();
        const preview = event.detail === 0
            ? projectAxisPointer(event.currentTarget, rect.top + rect.height / 2)
            : projectAxisPointer(event.currentTarget, event.clientY);
        if (preview !== null) {
            onCreateMarker(preview.memoryBytes, 'custom');
        }
    };

    return <Overlay data-testid="lifecycleMemoryMarkerOverlay">
        {projectedMarkers.map(marker => {
            const emphasized = marker.id === emphasizedMarkerId ||
                (activeGapMarkerIds?.includes(marker.id) ?? false);
            return <MarkerLine
                key={marker.id}
                markerColor={marker.color}
                $emphasized={emphasized}
                style={{ top: marker.screenY }}
                data-marker-id={marker.id}
                data-emphasized={String(emphasized)}
            />;
        })}
        {effectivePreview === null
            ? <></>
            : <>
                <AxisPreviewLine
                    data-testid="memoryMarkerAxisPreviewLine"
                    previewColor={effectivePreview.color}
                    style={{ top: effectivePreview.screenY }}
                />
                <AxisPreviewValue style={{ top: effectivePreview.screenY }}>
                    {formatBytes(effectivePreview.memoryBytes)}
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
            {effectivePreview === null
                ? <></>
                : <AxisPreviewFlag
                    data-testid="memoryMarkerAxisPreviewFlag"
                    style={{ top: effectivePreview.screenY, color: effectivePreview.color }}
                    data-preview-source={effectivePreview.source}
                ><TimelineFlagIcon aria-hidden="true" /></AxisPreviewFlag>}
            <AxisRail />
            {flagLayout.slice(1).map((layout, index) => {
                const previousLayout = flagLayout[index];
                const firstMarker = markerById.get(previousLayout.id);
                const secondMarker = markerById.get(layout.id);
                if (firstMarker === undefined || secondMarker === undefined) {
                    return <React.Fragment key={`${previousLayout.id}-${layout.id}`} />;
                }
                const isActive = activeGapMarkerIds?.[0] === previousLayout.id &&
                    activeGapMarkerIds[1] === layout.id;
                const difference = Math.abs(firstMarker.memoryBytes - secondMarker.memoryBytes);
                const linkedBlockIds = Array.from(new Set(
                    [firstMarker, secondMarker]
                        .filter(marker => marker.source === 'block' && marker.blockId !== undefined)
                        .map(marker => marker.blockId as number),
                ));
                return <AxisGapSegment
                    key={`${previousLayout.id}-${layout.id}`}
                    type="button"
                    $active={isActive}
                    aria-label={t('memoryMarkerGapBetween', {
                        first: getLifecycleMemoryMarkerLabel(firstMarker, 0),
                        second: getLifecycleMemoryMarkerLabel(secondMarker, 0),
                        value: formatBytes(difference),
                    })}
                    style={{
                        top: previousLayout.displayY,
                        height: Math.max(1, layout.displayY - previousLayout.displayY),
                    }}
                    data-testid="memoryMarkerGapSegment"
                    data-gap-start-marker={previousLayout.id}
                    data-gap-end-marker={layout.id}
                    data-active={String(isActive)}
                    onMouseEnter={(): void => handleGapHoverChange(
                        [previousLayout.id, layout.id],
                        linkedBlockIds,
                    )}
                    onMouseLeave={(): void => handleGapHoverChange(null)}
                    onFocus={(): void => handleGapHoverChange(
                        [previousLayout.id, layout.id],
                        linkedBlockIds,
                    )}
                    onBlur={(): void => handleGapHoverChange(null)}
                    onClick={(event): void => event.stopPropagation()}
                >
                    <InlineGapValue $active={isActive} data-testid="memoryMarkerInlineGapValue">
                        {`Δ ${formatBytes(difference)}`}
                    </InlineGapValue>
                </AxisGapSegment>;
            })}
            {projectedMarkers.map(marker => {
                const layout = layoutById.get(marker.id);
                const markerIndex = markerIndexById.get(marker.id) ?? 0;
                if (layout === undefined) {
                    return <React.Fragment key={marker.id} />;
                }
                const markerLabel = getLifecycleMemoryMarkerLabel(marker, markerIndex);
                const upperMarker = markerIndex < markers.length - 1 ? markers[markerIndex + 1] : null;
                const lowerMarker = markerIndex > 0 ? markers[markerIndex - 1] : null;
                const upperSpacing = getSpacing(markerIndex, 'upper');
                const lowerSpacing = getSpacing(markerIndex, 'lower');
                const connectorTop = Math.min(layout.actualY, layout.displayY);
                const connectorHeight = Math.abs(layout.displayY - layout.actualY);
                const emphasized = marker.id === emphasizedMarkerId ||
                    (activeGapMarkerIds?.includes(marker.id) ?? false);
                return <React.Fragment key={marker.id}>
                    <AxisTick
                        markerColor={marker.color}
                        $emphasized={emphasized}
                        style={{ top: layout.actualY }}
                        data-testid="memoryMarkerBaselineGuide"
                        data-marker-id={marker.id}
                        data-emphasized={String(emphasized)}
                    />
                    {connectorHeight > 1
                        ? <AxisConnector
                            markerColor={marker.color}
                            style={{ top: connectorTop, height: connectorHeight }}
                        />
                        : <></>}
                    <FlagPosition
                        style={{ top: layout.displayY }}
                        data-marker-ordinal={marker.ordinal}
                        onMouseEnter={(): void => handleMarkerHoverChange(marker)}
                        onMouseLeave={(event): void => {
                            if (!event.currentTarget.contains(document.activeElement)) {
                                handleMarkerHoverChange(null);
                            }
                        }}
                        onFocusCapture={(): void => handleMarkerHoverChange(marker)}
                        onBlurCapture={(event): void => {
                            if (!event.currentTarget.contains(event.relatedTarget as Node | null)) {
                                handleMarkerHoverChange(null);
                            }
                        }}
                    >
                        <MarkerFlag
                            type="button"
                            markerColor={marker.color}
                            $emphasized={emphasized}
                            aria-label={`${markerLabel}${marker.source === 'block' ? `, ${t('blockMemoryMarker')}` : ''}, ${t('memoryMarkerBaseline')}: ${formatBytes(marker.memoryBytes)}`}
                            onClick={(event): void => event.stopPropagation()}
                        ><TimelineFlagIcon aria-hidden="true" /></MarkerFlag>
                        <DeleteBadge
                            type="button"
                            data-marker-delete="true"
                            aria-label={`${t('deleteMemoryMarker')}: ${markerLabel}`}
                            onClick={(event): void => {
                                event.stopPropagation();
                                handleMarkerHoverChange(null);
                                session.deleteLifecycleMemoryMarker(marker.id);
                            }}
                        ><DeleteOutlined /></DeleteBadge>
                        <MarkerHoverCard
                            role="tooltip"
                            data-marker-details="true"
                            placement={getHoverCardPlacement(layout.displayY, viewport.height)}
                        >
                            {upperMarker !== null
                                ? <>
                                    <RelationNode>
                                        <RelationFlag
                                            markerColor={getLifecycleMemoryMarkerColor(upperMarker, markerIndex + 1)}
                                            data-testid="memoryMarkerRelationFlag"
                                        ><TimelineFlagIcon aria-hidden="true" /></RelationFlag>
                                        <RelationMarkerIdentity>{getLifecycleMemoryMarkerLabel(upperMarker, markerIndex + 1)}</RelationMarkerIdentity>
                                        <RelationBaselineValue data-testid="memoryMarkerRelationBaseline">{formatBytes(upperMarker.memoryBytes)}</RelationBaselineValue>
                                    </RelationNode>
                                    <RelationGap>
                                        <RelationStem />
                                        <RelationGapValue data-testid="memoryMarkerRelationGap">{`↑ ${formatSpacing(upperSpacing)}`}</RelationGapValue>
                                    </RelationGap>
                                </>
                                : <></>}
                            <RelationNode data-testid="memoryMarkerCurrentRelation">
                                <RelationFlag markerColor={marker.color} data-testid="memoryMarkerRelationFlag">
                                    <TimelineFlagIcon aria-hidden="true" />
                                </RelationFlag>
                                <RelationMarkerIdentity>{markerLabel}</RelationMarkerIdentity>
                                <RelationBaselineValue data-testid="memoryMarkerRelationBaseline">{formatBytes(marker.memoryBytes)}</RelationBaselineValue>
                            </RelationNode>
                            {lowerMarker !== null
                                ? <>
                                    <RelationGap>
                                        <RelationStem />
                                        <RelationGapValue data-testid="memoryMarkerRelationGap">{`↓ ${formatSpacing(lowerSpacing)}`}</RelationGapValue>
                                    </RelationGap>
                                    <RelationNode>
                                        <RelationFlag
                                            markerColor={getLifecycleMemoryMarkerColor(lowerMarker, markerIndex - 1)}
                                            data-testid="memoryMarkerRelationFlag"
                                        ><TimelineFlagIcon aria-hidden="true" /></RelationFlag>
                                        <RelationMarkerIdentity>{getLifecycleMemoryMarkerLabel(lowerMarker, markerIndex - 1)}</RelationMarkerIdentity>
                                        <RelationBaselineValue data-testid="memoryMarkerRelationBaseline">{formatBytes(lowerMarker.memoryBytes)}</RelationBaselineValue>
                                    </RelationNode>
                                </>
                                : <></>}
                            {upperMarker === null && lowerMarker === null
                                ? <NoNeighbor>{t('memoryMarkerNoNeighbor')}</NoNeighbor>
                                : <></>}
                        </MarkerHoverCard>
                    </FlagPosition>
                </React.Fragment>;
            })}
        </MarkerAxis>
    </Overlay>;
});
