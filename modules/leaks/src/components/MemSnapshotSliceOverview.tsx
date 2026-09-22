/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React, { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import styled from '@emotion/styled';
import { ResizeTable } from '@insight/lib/resize';
import type { ColumnsType } from 'antd/es/table';
import { Tooltip } from 'antd';
import { Painter } from '../leaksWorker/blockWorker/nativeCanvas/Painter';
import { useTranslation } from 'react-i18next';
import type { MemSnapshotDeviceSliceInfo } from '@/entity/session';
import type { AllocationData } from '@/utils/RequestUtils';
import { formatBytes } from '../utils/utils';
import { resolveLifecycleKeyboardAction } from './leaks/lifecycleNavigation';
import { OverviewSplitPane } from './OverviewSplitPane';
import { animateOverviewOpening, type OverviewTransition } from './overviewOpeningAnimation';
import { OverviewZoomControls } from './OverviewZoomControls';
import { overviewY, sampleOverview, OverviewTimelineIndex, getOverviewSummary, type OverviewPoint } from './snapshotOverview';

const OverviewTrack = styled.div`
    display: flex;
    width: 100%;
    min-width: 720px;
    overflow-x: auto;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 3px;

    .slice-range-handle {
        opacity: 0;
        transition: opacity 0.15s ease;
    }

    &:hover .slice-range-handle,
    &:focus-within .slice-range-handle,
    .slice-range-handle:active {
        opacity: 1;
    }
`;

const CompactOverview = styled.div`
    position: relative;
    &[data-floating='true'] { opacity: 0; }
    &:hover .overview-expand-button, &:focus-within .overview-expand-button { opacity: 1; pointer-events: auto; }
    @media (hover: none) { .overview-expand-button { opacity: 1; pointer-events: auto; } }
`;

const SliceBlock = styled.div`
    position: relative;
    display: flex;
    flex: 1 1 0;
    min-width: 48px;
    height: 72px;
    padding: 18px 0 17px;
    overflow: hidden;
    border-right: 1px solid ${(props): string => props.theme.borderColor};
    background: ${(props): string => props.theme.bgColorCommon};
    color: ${(props): string => props.theme.textColorSecondary};
    cursor: pointer;

    &:last-child {
        border-right: 0;
    }

    &[data-selected='true'] {
        z-index: 1;
        box-shadow: inset 0 0 0 2px ${(props): string => props.theme.primaryColor};
    }

    &:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: -2px;
    }

    &[data-ready='false'] {
        background: repeating-linear-gradient(
            -45deg,
            ${(props): string => props.theme.bgColorCommon},
            ${(props): string => props.theme.bgColorCommon} 6px,
            ${(props): string => props.theme.bgColorLight} 6px,
            ${(props): string => props.theme.bgColorLight} 12px
        );
        cursor: not-allowed;
    }
`;

const SliceLabel = styled.span`
    position: absolute;
    top: 2px;
    left: 6px;
    right: 6px;
    overflow: hidden;
    font-size: 11px;
    line-height: 14px;
    text-align: left;
    text-overflow: ellipsis;
    white-space: nowrap;
`;

const EmptySlice = styled.span`
    display: flex;
    width: 100%;
    align-items: center;
    justify-content: center;
    font-size: 11px;
`;

const SliceTrend = styled.div`
    position: relative;
    width: 100%;
    height: 36px;

    > svg {
        display: block;
    }

    .slice-trend-area {
        fill: ${(props): string => props.theme.primaryColor};
        opacity: ${(props): number => props.theme.mode === 'dark' ? 0.32 : 0.14};
    }

    .slice-trend-line {
        fill: none;
        stroke: ${(props): string => props.theme.mode === 'dark' ? props.theme.primaryColorDark : '#516489'};
    }
`;

const AxisCoordinate = styled.span`
    position: absolute;
    bottom: 2px;
    z-index: 2;
    font-size: 10px;
    line-height: 12px;
    color: ${(props): string => props.theme.textColorSecondary};
    pointer-events: none;
    white-space: nowrap;
`;

const SliceStartCoordinate = styled(AxisCoordinate)`
    left: 4px;
`;

const SliceEndCoordinate = styled(AxisCoordinate)`
    right: 4px;
`;

const SliceZoomOverlay = styled.div`
    position: absolute;
    inset: 0;
`;

const SelectedRange = styled.div`
    position: absolute;
    top: 0;
    bottom: 0;
    border: 1px solid ${(props): string => props.theme.primaryColor};
    background: rgba(24, 144, 255, 0.12);
    cursor: grab;

    &:active {
        cursor: grabbing;
    }
`;

const RangeHandle = styled.div`
    position: absolute;
    top: 4px;
    bottom: 4px;
    width: 7px;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 2px;
    background: ${(props): string => props.theme.bgColorCommon};
    box-shadow: 0 0 2px rgba(0, 0, 0, 0.2);
    cursor: ew-resize;
    transform: translateX(-50%);
`;

const OverviewButton = styled.button`
    padding: 4px 8px;
    border: 1px solid ${(props): string => props.theme.primaryColor};
    border-radius: 3px;
    color: ${(props): string => props.theme.mode === 'dark' ? props.theme.primaryColorDark : props.theme.primaryColor};
    background: ${(props): string => props.theme.bgColorCommon};
    font: inherit;
    cursor: pointer;
    &:focus-visible { outline: 2px solid currentColor; outline-offset: 2px; }
`;

const ExpandButton = styled(OverviewButton)`
    position: absolute;
    top: 4px;
    right: 6px;
    z-index: 3;
    display: grid;
    place-items: center;
    width: 24px;
    height: 24px;
    padding: 3px;
    opacity: 0;
    pointer-events: none;
    box-shadow: 0 2px 6px rgba(0, 0, 0, 0.14);
    &:hover { background: ${(props): string => props.theme.bgColorLight}; }
`;

const CloseButton = styled.button`
    position: absolute;
    top: 6px;
    right: 8px;
    display: grid;
    place-items: center;
    width: 24px;
    height: 24px;
    padding: 3px;
    border: 0;
    border-radius: 3px;
    color: ${(props): string => props.theme.textColorSecondary};
    background: transparent;
    cursor: pointer;
    &:hover { background: ${(props): string => props.theme.bgColorLight}; }
    &:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; }
`;

const PeakTarget = styled.button`
    position: absolute;
    z-index: 3;
    width: 12px;
    height: 12px;
    padding: 0;
    border: 0;
    border-radius: 2px;
    font-size: 10px;
    line-height: 14px;
    white-space: nowrap;
    color: ${(props): string => props.theme.mode === 'dark' ? '#FFBB66' : '#854000'};
    background: transparent;
    transform: translate(-50%, -50%);
    cursor: pointer;
    > svg { display: block; fill: currentColor; stroke: ${(props): string => props.theme.bgColorCommon}; stroke-width: 1.5; }
    &:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; }
`;

const PeakLegend = styled.div`
    position: absolute;
    top: 8px;
    left: 10px;
    z-index: 3;
    display: flex;
    align-items: center;
    gap: 4px;
    padding: 2px 5px;
    border-radius: 3px;
    font-size: 11px;
    color: ${(props): string => props.theme.textColorSecondary};
    background: ${(props): string => props.theme.bgColorCommon};
    pointer-events: none;
    svg { color: ${(props): string => props.theme.mode === 'dark' ? '#FFBB66' : '#854000'}; }
`;

const WindowBoundary = styled.div`
    position: absolute;
    top: 0;
    bottom: 0;
    width: 0;
    z-index: 1;
    border-left: 1px dashed ${(props): string => props.theme.textColorSecondary};
    pointer-events: none;
`;

const OverviewDialog = styled.dialog`
    width: min(1440px, calc(100vw - 48px));
    margin: auto;
    max-width: none;
    box-sizing: border-box;
    height: min(800px, calc(100dvh - 48px));
    max-height: calc(100dvh - 24px);
    &[open] { display: flex; flex-direction: column; gap: 8px; }
    padding: 36px 24px 20px;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 6px;
    color: ${(props): string => props.theme.textColorPrimary};
    background: ${(props): string => props.theme.bgColorCommon};
    overflow: auto;
    &::backdrop { background: rgba(0, 0, 0, 0.45); }
    &[data-entering='true'] { opacity: 0; }
    &[data-entering='true']::backdrop, &[data-opening='true']::backdrop { background: transparent; }
    &[data-opening='true'] { background: transparent; border-color: transparent; overflow: visible; }
    &[data-opening='true'] > :not(.overview-opening-surface):not(.overview-opening-strip):not(.overview-opening-backdrop) { pointer-events: none; z-index: 1; }
    &[data-opening='true'] > [data-testid='overviewNavigator'] { z-index: 3; }
    .overview-opening-surface { position: fixed; pointer-events: none; overflow: hidden; box-sizing: border-box;
        border: 1px solid ${(props): string => props.theme.borderColor}; border-radius: 6px;
        background: ${(props): string => props.theme.bgColorCommon}; z-index: 0; }
    .overview-opening-strip { position: fixed; pointer-events: none; overflow: hidden; z-index: 2; }
    .overview-opening-backdrop { position: fixed; inset: 0; background: rgba(0, 0, 0, 0.45); pointer-events: none; z-index: -1; }
    .slice-range-handle { opacity: 1; }
    .expanded-overview { min-width: 0; flex-shrink: 0; }
    &, * {
        scrollbar-width: thin;
        scrollbar-color: ${(props): string => props.theme.scrollbarColor} transparent;
    }
    ::-webkit-scrollbar { width: 6px; height: 6px; }
    ::-webkit-scrollbar-thumb { background: ${(props): string => props.theme.scrollbarColor}; border-radius: 6px; }
    ::-webkit-scrollbar-track { background: transparent; }
    @media (max-width: 900px) { padding: 36px 12px 12px; width: calc(100vw - 24px); }
`;

const OverviewCanvas = styled.div`
    position: relative;
    flex: 1;
    min-width: 0;
    min-height: 0;
    overflow: auto;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 3px;
    background: ${(props): string => props.theme.bgColorLight};
    touch-action: none;
    user-select: none;
    cursor: grab;
    &[data-dragging='true'] { cursor: grabbing; }
`;

const TrendSurface = styled.div`
    position: relative;
    box-sizing: border-box;
    padding: 28px 0 24px;
    background: ${(props): string => props.theme.bgColorCommon};
`;

const Navigator = styled(TrendSurface)`
    flex-shrink: 0;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: 3px;
    padding: 20px 0 18px;
    touch-action: none;
`;

const NavigatorLabel = styled.span`
    position: absolute;
    top: 2px;
    font-size: 11px;
    line-height: 14px;
    color: ${(props): string => props.theme.textColorSecondary};
    background: ${(props): string => props.theme.bgColorCommon};
    pointer-events: none;
    white-space: nowrap;
    &[data-vertical='true'] { writing-mode: vertical-rl; text-orientation: upright; }
`;

const WindowCell = styled.div`
    display: flex;
    align-items: center;
    justify-content: flex-start;
    min-height: 24px;
    gap: 4px;
    .window-switch { opacity: 0; pointer-events: none; }
    > span { white-space: nowrap; }
    .window-identity { display: flex; align-items: center; gap: 5px; }
    .window-action-slot { display: inline-flex; align-items: center; justify-content: center; flex: 0 0 24px; height: 24px; }
    .active-window-marker { display: inline-flex; align-items: center; justify-content: center; width: 16px; height: 16px;
        color: ${(props): string => props.theme.primaryColor}; border-radius: 4px; }
    .active-window-marker:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; outline-offset: 2px; }
    @media (hover: none) { .window-switch { opacity: 1; pointer-events: auto; } }
`;

const WindowSwitch = styled.button`
    flex-shrink: 0;
    display: grid;
    place-items: center;
    width: 24px;
    height: 24px;
    padding: 3px;
    border: 0;
    border-radius: 3px;
    background: transparent;
    color: ${(props): string => props.theme.textColorSecondary};
    cursor: pointer;
    &:hover { color: ${(props): string => props.theme.primaryColor}; background: ${(props): string => props.theme.bgColorLight}; }
    &:focus-visible { outline: 2px solid ${(props): string => props.theme.primaryColor}; }
`;

const RankingPanel = styled.div`
    flex: 1;
    display: flex;
    flex-direction: column;
    min-height: 0;
    min-width: 0;
`;

const PeakRanking = styled.div`
    flex: 1;
    min-height: 0;
    overflow: auto;
    .ant-table-tbody > tr { cursor: pointer; }
    .ant-table-tbody > tr:hover .window-switch,
    .ant-table-tbody > tr:focus-visible .window-switch,
    .ant-table-tbody .window-switch:focus-visible { opacity: 1; pointer-events: auto; }
    && .ant-table-tbody > tr.overview-selected-row > td.ant-table-cell,
    && .ant-table-tbody > tr.overview-selected-row:hover > td.ant-table-cell {
        background: ${(props): string => props.theme.primaryColorLight4};
    }
`;

interface PeakRow { index: number; peak: number; event: number; active: boolean; activate: () => void }

const EMPTY_POINTS: OverviewPoint[] = [];
const getOverviewPopupContainer = (node: HTMLElement): HTMLElement => node.closest('dialog') ?? document.body;

interface CurveProps {
    points: OverviewPoint[];
    index?: OverviewTimelineIndex;
    start: number;
    end: number;
    min: number;
    max: number;
    height: number;
    adaptive?: boolean;
    measureReady?: boolean;
    nativeCanvas?: boolean;
    children?: React.ReactNode;
}

const OverviewCurve = React.memo(({ points, index, start, end, min, max, height: initialHeight, adaptive = false, measureReady = true, nativeCanvas = false, children }: CurveProps): React.ReactElement => {
    const ref = useRef<HTMLDivElement>(null);
    const paintRef = useRef<HTMLCanvasElement>(null);
    const painterRef = useRef<{ painter: Painter; ready: Promise<void>; canvas: HTMLCanvasElement; ratio: number }>();
    const [width, setWidth] = useState(0);
    const [measuredHeight, setMeasuredHeight] = useState(initialHeight);
    const height = adaptive ? measuredHeight : initialHeight;
    useLayoutEffect(() => {
        if (!measureReady) return;
        const element = ref.current;
        if (!element) return;
        const resize = (): void => {
            const layoutWidth = element.clientWidth || element.getBoundingClientRect().width;
            const layoutHeight = element.clientHeight;
            if (layoutWidth <= 0) return;
            setWidth(Math.max(1, Math.floor(layoutWidth)));
            if (adaptive && layoutHeight > 0) setMeasuredHeight(Math.max(1, Math.floor(layoutHeight)));
        };
        resize();
        if (typeof ResizeObserver === 'undefined') return;
        const observer = new ResizeObserver(resize);
        observer.observe(element);
        return () => observer.disconnect();
    }, [adaptive, measureReady]);
    const sampled = useMemo(() => measureReady && width > 0 ? (index ? index.sample(start, end, width) : sampleOverview(points, start, end, width)) : EMPTY_POINTS,
        [points, index, start, end, width, measureReady]);
    useLayoutEffect(() => {
        const canvas = paintRef.current;
        if (!nativeCanvas || !measureReady || !canvas || width <= 0) return;
        const ratio = Math.min(window.devicePixelRatio || 1, 8192 / Math.max(width, height));
        const pixelWidth = Math.max(1, Math.floor(width * ratio));
        const pixelHeight = Math.max(1, Math.floor(height * ratio));
        if (canvas.width !== pixelWidth) canvas.width = pixelWidth;
        if (canvas.height !== pixelHeight) canvas.height = pixelHeight;
        if (!painterRef.current || painterRef.current.canvas !== canvas || painterRef.current.ratio !== ratio) {
            const painter = new Painter(canvas, ratio);
            painterRef.current = { painter, ready: painter.initialize(), canvas, ratio };
        }
        const { painter, ready } = painterRef.current;
        let disposed = false;
        void ready.then(async () => {
            if (disposed) return;
            painter.setAllocationLines({
                reservedLine: sampled.map(point => [point.timestamp, point.totalSize - min]),
                processUsedLine: [],
                deviceUsedLine: [],
            });
            await painter.render({
                transform: { x: 0, y: 4 * ratio, scaleX: 1, scaleY: 1 },
                viewport: { width: canvas.width, height: canvas.height },
                zoom: { x: canvas.width / Math.max(1, end - start), y: (canvas.height - 8 * ratio) / Math.max(1, max - min), offset: start },
            }, () => disposed, { blocks: false, overview: true });
        });
        return () => { disposed = true; };
    }, [nativeCanvas, measureReady, width, height, sampled, start, end, min, max]);
    const span = Math.max(1, end - start);
    const line = useMemo(() => nativeCanvas
        ? ''
        : sampled.map(point =>
            `${(point.timestamp - start) / span * width},${overviewY(point.totalSize, min, max, height)}`,
        ).join(' '), [nativeCanvas, sampled, start, span, width, min, max, height]);
    const firstX = sampled.length > 0 ? (sampled[0].timestamp - start) / span * width : 0;
    const lastX = sampled.length > 0 ? (sampled[sampled.length - 1].timestamp - start) / span * width : 0;
    return <SliceTrend ref={ref} style={adaptive ? { flex: 1, minHeight: 160, height: 0 } : { height }}>
        {nativeCanvas
            ? <canvas ref={paintRef} style={{ display: 'block', width: '100%', height }} aria-hidden="true" />
            : <svg width="100%" height={height} viewBox={`0 0 ${Math.max(1, width)} ${height}`} preserveAspectRatio="none" aria-hidden="true">
                <polygon className="slice-trend-area" points={line ? `${firstX},${height} ${line} ${lastX},${height}` : ''} />
                <polyline className="slice-trend-line" strokeWidth="1.25" vectorEffect="non-scaling-stroke" points={line} />
            </svg>}
        {children}
    </SliceTrend>;
});
OverviewCurve.displayName = 'OverviewCurve';

type DragMode = 'start' | 'end' | 'range';

const RANGE_CHANGE_DEBOUNCE_MS = 150;
const formatEventId = (eventId: number): string => String(Math.trunc(eventId));
const normalizeRange = (
    minTime: number,
    maxTime: number,
    selectedStart?: number,
    selectedEnd?: number,
): [number, number] => {
    const start = Math.max(minTime, Math.min(maxTime, selectedStart ?? minTime));
    const end = Math.max(minTime, Math.min(maxTime, selectedEnd ?? maxTime));
    return start < end || minTime === maxTime ? [start, end] : [minTime, maxTime];
};

interface SliceRangeSelectorProps {
    minTime: number;
    maxTime: number;
    selectedRange?: [number, number];
    onRangeChange: (range: [number, number]) => void;
    flushRef?: React.MutableRefObject<(() => void) | null>;
    immediate?: boolean;
}

const SliceRangeSelector = ({
    minTime,
    maxTime,
    selectedRange,
    onRangeChange,
    flushRef,
    immediate = false,
}: SliceRangeSelectorProps): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const trackRef = useRef<HTMLDivElement>(null);
    const dragRef = useRef<{
        mode: DragMode;
        originX: number;
        originRange: [number, number];
        trackWidth: number;
    } | undefined>(undefined);
    const rangeChangeTimerRef = useRef<number | undefined>(undefined);
    const onRangeChangeRef = useRef(onRangeChange);
    const pendingRangeRef = useRef<[number, number] | null>(null);
    const flushRange = useCallback((): void => {
        if (rangeChangeTimerRef.current !== undefined) window.clearTimeout(rangeChangeTimerRef.current);
        rangeChangeTimerRef.current = undefined;
        const pending = pendingRangeRef.current;
        pendingRangeRef.current = null;
        if (pending) onRangeChangeRef.current(pending);
    }, []);
    useEffect(() => {
        if (flushRef) flushRef.current = flushRange;
        return () => { if (flushRef) flushRef.current = null; };
    }, [flushRef, flushRange]);
    const timeSpan = Math.max(1, maxTime - minTime);
    const minRange = immediate ? 1 : Math.max(1, Math.round(timeSpan / 100));
    const selectedStart = selectedRange?.[0];
    const selectedEnd = selectedRange?.[1];
    const [range, setRange] = useState<[number, number]>(() => (
        normalizeRange(minTime, maxTime, selectedStart, selectedEnd)
    ));
    const startPercent = (range[0] - minTime) / timeSpan * 100;
    const endPercent = (range[1] - minTime) / timeSpan * 100;

    useEffect(() => {
        onRangeChangeRef.current = onRangeChange;
    }, [onRangeChange]);

    useEffect(() => {
        if (rangeChangeTimerRef.current !== undefined) {
            window.clearTimeout(rangeChangeTimerRef.current);
            rangeChangeTimerRef.current = undefined;
        }
        pendingRangeRef.current = null;
        setRange(normalizeRange(minTime, maxTime, selectedStart, selectedEnd));
    }, [minTime, maxTime, selectedStart, selectedEnd]);

    useEffect(() => (): void => {
        if (rangeChangeTimerRef.current !== undefined) {
            window.clearTimeout(rangeChangeTimerRef.current);
        }
    }, []);

    const scheduleRangeChange = (nextRange: [number, number]): void => {
        setRange(nextRange);
        if (immediate) { onRangeChangeRef.current(nextRange); return; }
        pendingRangeRef.current = nextRange;
        if (rangeChangeTimerRef.current !== undefined) {
            window.clearTimeout(rangeChangeTimerRef.current);
        }
        rangeChangeTimerRef.current = window.setTimeout(flushRange, RANGE_CHANGE_DEBOUNCE_MS);
    };

    const beginDrag = (event: React.PointerEvent<HTMLDivElement>, mode: DragMode): void => {
        event.stopPropagation();
        const trackWidth = trackRef.current?.getBoundingClientRect().width ?? 0;
        if (trackWidth <= 0) {
            return;
        }
        dragRef.current = { mode, originX: event.clientX, originRange: range, trackWidth };
        event.currentTarget.setPointerCapture(event.pointerId);
    };

    const handlePointerMove = (event: React.PointerEvent<HTMLDivElement>): void => {
        const drag = dragRef.current;
        if (drag === undefined || !event.currentTarget.hasPointerCapture(event.pointerId)) {
            return;
        }
        event.stopPropagation();
        const delta = Math.round((event.clientX - drag.originX) / drag.trackWidth * timeSpan);
        const [originStart, originEnd] = drag.originRange;
        if (drag.mode === 'start') {
            scheduleRangeChange([Math.max(minTime, Math.min(originEnd - minRange, originStart + delta)), originEnd]);
            return;
        }
        if (drag.mode === 'end') {
            scheduleRangeChange([originStart, Math.min(maxTime, Math.max(originStart + minRange, originEnd + delta))]);
            return;
        }
        const rangeWidth = originEnd - originStart;
        const nextStart = Math.max(minTime, Math.min(maxTime - rangeWidth, originStart + delta));
        scheduleRangeChange([nextStart, nextStart + rangeWidth]);
    };

    const endDrag = (event: React.PointerEvent<HTMLDivElement>): void => {
        if (event.currentTarget.hasPointerCapture(event.pointerId)) {
            event.currentTarget.releasePointerCapture(event.pointerId);
        }
        dragRef.current = undefined;
        flushRange();
    };

    const handleKeyDown = (event: React.KeyboardEvent<HTMLDivElement>, mode: 'start' | 'end'): void => {
        const step = Math.max(1, Math.round(timeSpan / 100));
        if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') {
            return;
        }
        event.preventDefault();
        event.stopPropagation();
        const delta = event.key === 'ArrowLeft' ? -step : step;
        if (mode === 'start') {
            scheduleRangeChange([Math.max(minTime, Math.min(range[1] - minRange, range[0] + delta)), range[1]]);
        } else {
            scheduleRangeChange([range[0], Math.min(maxTime, Math.max(range[0] + minRange, range[1] + delta))]);
        }
    };

    return <SliceZoomOverlay ref={trackRef} onClick={(event): void => event.stopPropagation()}>
        <SelectedRange
            data-testid="sliceSelectedRange"
            style={{ left: `${startPercent}%`, width: `${Math.max(0, endPercent - startPercent)}%` }}
            onPointerDown={(event): void => beginDrag(event, 'range')}
            onPointerMove={handlePointerMove}
            onPointerUp={endDrag}
            onPointerCancel={endDrag}
        />
        <RangeHandle
            className="slice-range-handle"
            role="slider"
            aria-label={t('overviewRangeStart')}
            aria-valuemin={minTime}
            aria-valuemax={range[1] - minRange}
            aria-valuenow={range[0]}
            tabIndex={0}
            style={{ left: `clamp(4px, ${startPercent}%, calc(100% - 4px))` }}
            onPointerDown={(event): void => beginDrag(event, 'start')}
            onPointerMove={handlePointerMove}
            onPointerUp={endDrag}
            onPointerCancel={endDrag}
            onKeyDown={(event): void => handleKeyDown(event, 'start')}
        />
        <RangeHandle
            className="slice-range-handle"
            role="slider"
            aria-label={t('overviewRangeEnd')}
            aria-valuemin={range[0] + minRange}
            aria-valuemax={maxTime}
            aria-valuenow={range[1]}
            tabIndex={0}
            style={{ left: `clamp(4px, ${endPercent}%, calc(100% - 4px))` }}
            onPointerDown={(event): void => beginDrag(event, 'end')}
            onPointerMove={handlePointerMove}
            onPointerUp={endDrag}
            onPointerCancel={endDrag}
            onKeyDown={(event): void => handleKeyDown(event, 'end')}
        />
    </SliceZoomOverlay>;
};

interface Props {
    deviceSlices: MemSnapshotDeviceSliceInfo;
    overviewData: Record<number, AllocationData>;
    selectedSliceIndex: number;
    selectedRange?: [number, number];
    onSelectSlice: (sliceIndex: number) => void;
    onRangeChange: (range: [number, number]) => void;
}

const MemSnapshotSliceOverview = ({
    deviceSlices,
    overviewData,
    selectedSliceIndex,
    selectedRange,
    onSelectSlice,
    onRangeChange,
}: Props): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const [expanded, setExpanded] = useState(false);
    const [popupCreated, setPopupCreated] = useState(false);
    const backdropPressRef = useRef(false);
    const [popupSliceIndex, setPopupSliceIndex] = useState(selectedSliceIndex);
    const [dialogReady, setDialogReady] = useState(false);
    const rootRef = useRef<HTMLDivElement>(null);
    const compactRef = useRef<HTMLDivElement>(null);
    const openingBounds = useRef<DOMRect>();
    const transitionRef = useRef<OverviewTransition>();
    const canvasRef = useRef<HTMLDivElement>(null);
    const navigatorRef = useRef<HTMLDivElement>(null);
    const [navigatorWidth, setNavigatorWidth] = useState(1000);
    const [tableVisible, setTableVisible] = useState(true);
    const [dragging, setDragging] = useState(false);
    const chartDragRef = useRef<{ x: number; y: number; start: number; span: number; width: number; offsetY: number; height: number; valueSpan: number }>();
    const [view, setView] = useState<{ range?: [number, number]; zoomY: number; offsetY: number; zoomActivity: number }>({ zoomY: 1, offsetY: 0, zoomActivity: 0 });
    const viewRef = useRef(view);
    const viewFrame = useRef<number>();
    const previewRange = view.range;
    const zoomY = view.zoomY;
    const [canvasHeight, setCanvasHeight] = useState(400);
    const [xZoomMode, setXZoomMode] = useState(false);
    const [originalSize, setOriginalSize] = useState({ width: 1000, height: 400 });
    const [sortOrder, setSortOrder] = useState('window');
    const dialogRef = useRef<HTMLDialogElement>(null);
    const openButtonRef = useRef<HTMLButtonElement | null>(null);
    const compactFlushRef = useRef<(() => void) | null>(null);
    const summaries = useMemo(() => {
        const result = new Map<number, ReturnType<typeof getOverviewSummary>>();
        deviceSlices.slices.forEach(slice => {
            if (slice.ready && overviewData[slice.index]) {
                result.set(slice.index, getOverviewSummary(overviewData[slice.index].allocations));
            }
        });
        return result;
    }, [deviceSlices, overviewData]);
    let globalMaxSize = 0;
    let globalPeakSlice: number | undefined;
    summaries.forEach((summary, index) => {
        if (summary.peak && (globalPeakSlice === undefined || summary.max > globalMaxSize)) {
            globalMaxSize = summary.max;
            globalPeakSlice = index;
        }
    });
    const rankedSlices = useMemo(() => [...summaries.entries()].filter(([, summary]) => summary.peak).sort((a, b) => {
        const difference = sortOrder === 'ascending' ? a[1].max - b[1].max : b[1].max - a[1].max;
        if (sortOrder.startsWith('window')) return (a[0] - b[0]) * (sortOrder === 'window' ? 1 : -1);
        if (sortOrder.startsWith('event')) return ((a[1].peak?.timestamp ?? 0) - (b[1].peak?.timestamp ?? 0)) * (sortOrder === 'event' ? 1 : -1) || a[0] - b[0];
        return difference || a[0] - b[0];
    }), [summaries, sortOrder]);
    const overviewIndex = useMemo(() => new OverviewTimelineIndex(deviceSlices.slices.filter(slice => slice.ready).map(slice => ({
        points: overviewData[slice.index]?.allocations ?? EMPTY_POINTS, start: slice.startEventId, end: slice.endEventId,
    }))), [deviceSlices, overviewData]);
    const timelineStart = deviceSlices.slices[0]?.startEventId ?? 0;
    const timelineEnd = deviceSlices.slices[deviceSlices.slices.length - 1]?.endEventId ?? timelineStart;
    const timelineSpan = Math.max(1, timelineEnd - timelineStart);
    const range = normalizeRange(timelineStart, timelineEnd, previewRange?.[0], previewRange?.[1]);
    const rangeSpan = Math.max(1, range[1] - range[0]);
    const continuousHeight = Math.max(120, canvasHeight - 52);
    const valueMax = Math.max(1, globalMaxSize * 1.08);
    const previewMin = view.offsetY;
    const previewMax = previewMin + valueMax / zoomY;
    const commitView = useCallback((): void => {
        viewFrame.current = undefined;
        setView({ ...viewRef.current });
    }, []);
    const updateView = (patch: Partial<typeof view>, immediate = false): void => {
        viewRef.current = { ...viewRef.current, ...patch };
        if (immediate) {
            if (viewFrame.current !== undefined) cancelAnimationFrame(viewFrame.current);
            commitView();
        } else if (viewFrame.current === undefined) viewFrame.current = requestAnimationFrame(commitView);
    };
    useEffect(() => (): void => {
        if (viewFrame.current !== undefined) cancelAnimationFrame(viewFrame.current);
    }, []);
    const updatePreviewRange = (next: [number, number]): void => {
        const current = viewRef.current;
        const previous = current.range ?? [timelineStart, timelineEnd];
        const resized = Math.abs((next[1] - next[0]) - (previous[1] - previous[0])) > 0.000001;
        updateView({ range: next, zoomActivity: current.zoomActivity + (resized ? 1 : 0) });
    };
    useEffect(() => {
        const midpoint = (range[0] + range[1]) / 2;
        const slice = deviceSlices.slices.find(item => item.ready && midpoint >= item.startEventId && midpoint <= item.endEventId);
        if (slice) setPopupSliceIndex(slice.index);
    }, [range[0], range[1], deviceSlices]);
    useLayoutEffect(() => {
        const canvas = canvasRef.current;
        if (!dialogReady || !canvas) return;
        const resize = (): void => setCanvasHeight(canvas.clientHeight || canvas.getBoundingClientRect().height || originalSize.height);
        resize();
        if (typeof ResizeObserver === 'undefined') return;
        const observer = new ResizeObserver(resize);
        observer.observe(canvas);
        return () => observer.disconnect();
    }, [dialogReady, tableVisible, originalSize.height]);
    const globalPeaks = useMemo(() => deviceSlices.slices.flatMap(slice => {
        const summary = summaries.get(slice.index);
        const displayedPeak = formatBytes(globalMaxSize);
        if (!slice.ready || !summary?.peak || formatBytes(summary.max) !== displayedPeak) return [];
        // Match the displayed precision, retaining the actual highest point of each peak plateau.
        const unit = 1024 ** Math.min(4, Math.max(0, Math.floor(Math.log2(Math.max(1, globalMaxSize)) / 10)));
        const minimumCandidate = globalMaxSize - unit * 0.001;
        let candidate: OverviewPoint | undefined;
        const markedEvents = new Set<number>();
        const peaks: OverviewPoint[] = [];
        const flush = (): void => {
            if (candidate && !markedEvents.has(candidate.timestamp)) {
                peaks.push(candidate);
                markedEvents.add(candidate.timestamp);
            }
            candidate = undefined;
        };
        (overviewData[slice.index]?.allocations ?? EMPTY_POINTS).forEach(point => {
            const atPeak = point.timestamp >= slice.startEventId && point.timestamp <= slice.endEventId &&
                point.totalSize >= minimumCandidate && formatBytes(point.totalSize) === displayedPeak;
            if (!atPeak) flush();
            else if (!candidate || point.totalSize > candidate.totalSize) candidate = point;
        });
        flush();
        return peaks.map(point => ({ point, index: slice.index }));
    }), [deviceSlices, overviewData, summaries, globalMaxSize]);
    const minimumWindowWidth = navigatorWidth * Math.min(...deviceSlices.slices.map(slice =>
        Math.max(1, slice.endEventId - slice.startEventId + 1))) / (timelineSpan + 1);
    const verticalLabels = minimumWindowWidth < 56;
    const labelStride = Math.max(1, Math.ceil(16 / Math.max(1, minimumWindowWidth)));
    useLayoutEffect(() => {
        const element = navigatorRef.current;
        if (!dialogReady || !element) return;
        const resize = (): void => setNavigatorWidth(element.clientWidth || 1000);
        resize();
        if (typeof ResizeObserver === 'undefined') return;
        const observer = new ResizeObserver(resize);
        observer.observe(element);
        return () => observer.disconnect();
    }, [dialogReady]);
    const peakLabel = (peak?: OverviewPoint): string => peak
        ? `${t('overviewPeak')}: ${formatBytes(peak.totalSize)} · ${t('overviewEvent')}: ${formatEventId(peak.timestamp)}`
        : t('overviewNoData');
    const closePopup = (): void => {
        const finish = (): void => {
            dialogRef.current?.close();
            setExpanded(false);
            setDialogReady(false);
            openButtonRef.current?.focus();
        };
        if (transitionRef.current) transitionRef.current.close(finish);
        else finish();
    };
    useLayoutEffect(() => {
        if (!expanded) return;
        const dialog = dialogRef.current;
        if (dialog) dialog.dataset.entering = 'true';
        dialog?.showModal();
        setDialogReady(true);
        return () => { if (dialog?.open) dialog.close(); };
    }, [expanded]);
    useLayoutEffect(() => {
        const dialog = dialogRef.current;
        const source = openingBounds.current;
        if (!expanded || !dialogReady || !dialog || !source || !compactRef.current) return;
        const transition = animateOverviewOpening(dialog, compactRef.current, source);
        transitionRef.current = transition;
        return () => { transition.dispose(); transitionRef.current = undefined; };
    }, [expanded, dialogReady]);
    const selectPopupSlice = (index: number): void => {
        setPopupSliceIndex(index);
        const slice = deviceSlices.slices.find(item => item.index === index);
        if (slice) updateView({ range: [slice.startEventId, slice.endEventId], zoomY: 1, offsetY: 0 }, true);
    };
    const resetView = (): void => {
        chartDragRef.current = undefined;
        setDragging(false);
        updateView({ range: [timelineStart, timelineEnd], zoomY: 1, offsetY: 0, zoomActivity: viewRef.current.zoomActivity + 1 }, true);
    };
    const openPopup = (button: HTMLButtonElement, index?: number): void => {
        openButtonRef.current = button;
        openingBounds.current = compactRef.current?.getBoundingClientRect();
        if (index !== undefined) selectPopupSlice(index);
        else if (!popupCreated) updateView({ range: [timelineStart, timelineEnd], zoomY: 1, offsetY: 0 }, true);
        if (!popupCreated) {
            const bounds = rootRef.current?.previousElementSibling?.getBoundingClientRect();
            setOriginalSize({ width: bounds && bounds.width > 0 ? bounds.width : 1000, height: bounds && bounds.height > 0 ? bounds.height : 400 });
        }
        setPopupCreated(true);
        setExpanded(true);
    };
    const renderPeak = (point: OverviewPoint, index: number, start: number, end: number, min: number, max: number, height: number, compact = false): React.ReactElement => {
        const left = (point.timestamp - start) / Math.max(1, end - start) * 100;
        const top = overviewY(point.totalSize, min, max, height);
        return <React.Fragment key={`${index}-${point.timestamp}`}>
            <PeakTarget data-testid="globalPeakMarker" style={{ left: `${left}%`, top }}
                aria-label={`${peakLabel(point)} · ${t('snapshotWindow')} ${index + 1}`} title={peakLabel(point)}
                onPointerDown={(event): void => event.stopPropagation()} onKeyDown={(event): void => event.stopPropagation()}
                onClick={(event): void => {
                    event.stopPropagation();
                    if (compact) openPopup(event.currentTarget, index);
                    else selectPopupSlice(index);
                }}>
                <svg data-testid="globalPeakPoint" width="12" height="12" viewBox="0 0 12 12" aria-hidden="true"><path d="M6 1 11 6 6 11 1 6Z" /></svg>
            </PeakTarget>
        </React.Fragment>;
    };
    const peakColumns = useMemo<ColumnsType<PeakRow>>(() => [
        {
            title: t('snapshotWindow'),
            dataIndex: 'index',
            key: 'window',
            width: 120,
            sorter: { compare: (a, b): number => a.index - b.index },
            sortOrder: sortOrder === 'window' ? 'ascend' : sortOrder === 'window-desc' ? 'descend' : null,
            render: (index: number, row: PeakRow): React.ReactNode => <WindowCell>
                <span className="window-action-slot">
                    {row.active
                        ? <Tooltip title={t('overviewActiveWindow')} mouseEnterDelay={0} mouseLeaveDelay={0}
                            trigger={['hover', 'focus']} getPopupContainer={getOverviewPopupContainer}>
                            <span className="active-window-marker" role="img" tabIndex={0} aria-label={t('overviewActiveWindow')}>
                                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" aria-hidden="true">
                                    <rect x="3" y="4" width="18" height="13" rx="2" />
                                    <path d="M8 21h8m-4-4v4" /><rect x="6" y="7" width="12" height="7" rx="0.5" fill="currentColor" stroke="none" />
                                </svg>
                            </span>
                        </Tooltip>
                        : <Tooltip title={t('overviewActivateWindow')} mouseEnterDelay={0} mouseLeaveDelay={0}
                            trigger={['hover', 'focus']} getPopupContainer={getOverviewPopupContainer}>
                            <WindowSwitch className="window-switch" aria-label={`${t('overviewActivateWindow')} · ${index + 1}`}
                                onClick={(event): void => {
                                    event.stopPropagation();
                                    row.activate();
                                }}>
                                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" aria-hidden="true">
                                    <rect x="3" y="4" width="18" height="13" rx="2" />
                                    <path d="M8 21h8m-4-4v4" /><rect x="6" y="7" width="12" height="7" rx="0.5" fill="currentColor" stroke="none" />
                                </svg>
                            </WindowSwitch>
                        </Tooltip>}
                </span>
                <span className="window-identity">{`${t('snapshotWindow')} ${index + 1}`}</span>
            </WindowCell>,
        },
        {
            title: t('overviewWindowMemoryPeak'),
            dataIndex: 'peak',
            key: 'peak',
            width: 150,
            sorter: { compare: (a, b): number => a.peak - b.peak },
            sortOrder: sortOrder === 'ascending' ? 'ascend' : sortOrder === 'descending' ? 'descend' : null,
            render: (value: number): string => formatBytes(value),
        },
        {
            title: t('overviewEvent'),
            dataIndex: 'event',
            key: 'event',
            width: 110,
            sorter: { compare: (a, b): number => a.event - b.event },
            sortOrder: sortOrder === 'event' ? 'ascend' : sortOrder === 'event-desc' ? 'descend' : null,
        },
    ], [t, sortOrder]);
    // ResizeTable caches column renderers; changing activation state must travel with row data.
    const rankingRows = useMemo(() => rankedSlices.map(([index, summary]) => ({
        index,
        peak: summary.max,
        event: summary.peak?.timestamp ?? 0,
        active: index === selectedSliceIndex,
        activate: (): void => { if (index !== selectedSliceIndex) onSelectSlice(index); },
    })), [rankedSlices, selectedSliceIndex, onSelectSlice]);
    const rankingTable = useMemo(() => (
        <ResizeTable<PeakRow>
            columns={peakColumns}
            dataSource={rankingRows}
            rowKey="index" pagination={false} scroll={{ x: 380 }}
            rowClassName={(row): string => row.index === popupSliceIndex ? 'overview-selected-row' : ''}
            onRow={(row) => ({
                tabIndex: 0,
                'aria-selected': row.index === popupSliceIndex,
                onClick: (): void => selectPopupSlice(row.index),
                onKeyDown: (event): void => {
                    if (event.target === event.currentTarget && (event.key === 'Enter' || event.key === ' ')) {
                        event.preventDefault();
                        selectPopupSlice(row.index);
                    }
                },
            })}
            onChange={(_pagination, _filters, sorter): void => {
                const current = Array.isArray(sorter) ? sorter[0] : sorter;
                const field = String(current.field);
                setSortOrder(!current.order
                    ? 'window'
                    : field === 'peak'
                        ? current.order === 'ascend' ? 'ascending' : 'descending'
                        : `${field === 'index' ? 'window' : 'event'}${current.order === 'descend' ? '-desc' : ''}`);
            }}
        />
    ), [peakColumns, rankingRows, popupSliceIndex]);
    const movePreview = (start: number, span: number): void => {
        const width = Math.max(1, Math.min(timelineSpan, span));
        const left = Math.max(timelineStart, Math.min(timelineEnd - width, start));
        updatePreviewRange([left, left + width]);
    };
    // Overscan grows with vertical zoom; the minimum scale has no blank panning area.
    const clampVerticalOffset = (offset: number, span: number): number => {
        const padding = Math.min(span * 0.08, Math.max(0, valueMax - span) * 0.08);
        return Math.max(-padding, Math.min(valueMax + padding - span, offset));
    };
    const beginChartDrag = (event: React.PointerEvent<HTMLDivElement>): void => {
        if (event.button === 1) { event.preventDefault(); resetView(); return; }
        if (event.button !== 0) return;
        const canvas = event.currentTarget;
        canvas.focus();
        event.preventDefault();
        chartDragRef.current = {
            x: event.clientX,
            y: event.clientY,
            start: range[0],
            span: rangeSpan,
            width: Math.max(1, canvas.getBoundingClientRect().width),
            offsetY: viewRef.current.offsetY,
            height: continuousHeight,
            valueSpan: valueMax / viewRef.current.zoomY,
        };
        canvas.setPointerCapture(event.pointerId);
        setDragging(true);
    };
    const moveChartDrag = (event: React.PointerEvent<HTMLDivElement>): void => {
        const drag = chartDragRef.current;
        if (!drag || !event.currentTarget.hasPointerCapture(event.pointerId)) return;
        movePreview(drag.start - (event.clientX - drag.x) / drag.width * drag.span, drag.span);
        updateView({
            offsetY: clampVerticalOffset(drag.offsetY + (event.clientY - drag.y) / drag.height * drag.valueSpan, drag.valueSpan),
        });
    };
    const endChartDrag = (event: React.PointerEvent<HTMLDivElement>): void => {
        if (viewFrame.current !== undefined) { cancelAnimationFrame(viewFrame.current); commitView(); }
        chartDragRef.current = undefined;
        setDragging(false);
        if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
    };
    const scale = (direction: number, horizontal: boolean, anchor = 0.5, y = (canvasRef.current?.clientHeight ?? canvasHeight) / 2): void => {
        updateView({ zoomActivity: viewRef.current.zoomActivity + 1 });
        const factor = direction > 0 ? 1.15 : 1 / 1.15;
        const current = viewRef.current;
        const currentRange = normalizeRange(timelineStart, timelineEnd, current.range?.[0], current.range?.[1]);
        const currentSpan = Math.max(1, currentRange[1] - currentRange[0]);
        const span = Math.max(1, Math.min(timelineSpan, currentSpan / factor));
        movePreview(currentRange[0] + (currentSpan - span) * anchor, span);
        if (!horizontal) {
            const nextY = Math.max(1, Math.min(4, current.zoomY * factor));
            const fraction = 1 - Math.max(0, Math.min(1, (y - 28) / continuousHeight));
            updateView({
                zoomY: nextY,
                offsetY: clampVerticalOffset(current.offsetY + fraction * (valueMax / current.zoomY - valueMax / nextY), valueMax / nextY),
            });
        }
    };
    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const wheel = (event: WheelEvent): void => {
            event.preventDefault();
            const bounds = canvas.getBoundingClientRect();
            scale(event.deltaY > 0 ? -1 : 1, event.shiftKey || (!event.ctrlKey && xZoomMode),
                Math.max(0, Math.min(1, (event.clientX - bounds.left) / Math.max(1, bounds.width))), event.clientY - bounds.top);
        };
        const keyboard = (event: KeyboardEvent): void => {
            if (event.isComposing || event.altKey) return;
            const key = event.key.toLowerCase();
            if (!event.ctrlKey && !event.metaKey && !event.shiftKey && (key === 'r' || key === 'h')) {
                event.preventDefault();
                if (key === 'h') setXZoomMode(mode => !mode);
                else resetView();
                return;
            }
            if ((event.ctrlKey || event.shiftKey) && ['+', '=', '-', '_'].includes(key)) {
                event.preventDefault();
                scale(key === '-' || key === '_' ? -1 : 1, event.shiftKey && !event.ctrlKey);
                return;
            }
            const action = resolveLifecycleKeyboardAction(event);
            if (!action) return;
            event.preventDefault();
            event.stopPropagation();
            if (action.startsWith('zoom')) scale(action.endsWith('in') ? 1 : -1, action.startsWith('zoom-x'));
            else {
                const current = viewRef.current;
                const currentRange = current.range ?? range;
                const span = currentRange[1] - currentRange[0];
                if (action === 'pan-left') movePreview(currentRange[0] - span * 0.1, span);
                if (action === 'pan-right') movePreview(currentRange[0] + span * 0.1, span);
                if (action === 'pan-up' || action === 'pan-down') {
                    updateView({
                        offsetY: clampVerticalOffset(current.offsetY + (action === 'pan-up' ? 1 : -1) * valueMax / current.zoomY * 0.1, valueMax / current.zoomY),
                    });
                }
            }
        };
        canvas.addEventListener('wheel', wheel, { passive: false });
        canvas.addEventListener('keydown', keyboard);
        return () => { canvas.removeEventListener('wheel', wheel); canvas.removeEventListener('keydown', keyboard); };
    }, [popupCreated, range[0], range[1], zoomY, xZoomMode, timelineStart, timelineEnd, popupSliceIndex, valueMax, continuousHeight]);
    const renderTrack = (): React.ReactElement => <OverviewTrack
        data-testid="snapshotSliceOverview">
        {deviceSlices.slices.map((slice, index) => {
            const data = overviewData[slice.index];
            const summary = summaries.get(slice.index);
            const isSelected = slice.index === selectedSliceIndex;
            const span = Math.max(1, slice.endEventId - slice.startEventId);
            const boundaryEventId = deviceSlices.slices[index + 1]?.startEventId ?? slice.endEventId;
            const range = normalizeRange(slice.startEventId, slice.endEventId, selectedRange?.[0], selectedRange?.[1]);
            const label = `${t('snapshotWindow')} ${slice.index + 1}`;
            const accessibleLabel = `${label} · ${!slice.ready ? t('pending') : summary ? peakLabel(summary.peak) : t('overviewLoading')}`;
            return <SliceBlock key={slice.index}
                role="button" tabIndex={slice.ready ? 0 : -1} aria-disabled={!slice.ready} aria-pressed={isSelected}
                aria-label={accessibleLabel}
                data-selected={String(isSelected)} data-ready={String(slice.ready)} data-global-peak={String(globalPeaks.some(item => item.index === slice.index))}
                style={{
                    flexGrow: Math.max(1, slice.endEventId - slice.startEventId + 1),
                }}
                onClick={(): void => { if (slice.ready) { onSelectSlice(slice.index); } }}
                onKeyDown={(event): void => {
                    if (slice.ready && (event.key === 'Enter' || event.key === ' ')) {
                        event.preventDefault();
                        onSelectSlice(slice.index);
                    }
                }}>
                <SliceLabel>{`${label}${slice.ready ? '' : ` · ${t('pending')}`}`}</SliceLabel>
                {!slice.ready
                    ? <EmptySlice>{t('pending')}</EmptySlice>
                    : !summary?.peak
                        ? <EmptySlice>{data ? t('overviewNoData') : t('overviewLoading')}</EmptySlice>
                        : <OverviewCurve points={data.allocations} start={slice.startEventId} end={slice.endEventId}
                            min={0} max={Math.max(1, globalMaxSize)} height={36}>
                            {isSelected && (!expanded && slice.endEventId > slice.startEventId
                                ? <SliceRangeSelector key={slice.index} minTime={slice.startEventId} maxTime={slice.endEventId}
                                    selectedRange={selectedRange} onRangeChange={onRangeChange} flushRef={compactFlushRef} />
                                : <SelectedRange style={{
                                    left: `${(range[0] - slice.startEventId) / span * 100}%`,
                                    width: `${Math.max(0, range[1] - range[0]) / span * 100}%`,
                                    pointerEvents: 'none',
                                }} />)}
                            {globalPeaks.filter(item => item.index === slice.index).map(({ point, index }) =>
                                renderPeak(point, index, slice.startEventId, slice.endEventId, 0, Math.max(1, globalMaxSize), 36, true))}
                        </OverviewCurve>}
                {index === 0 && <SliceStartCoordinate data-testid="sliceAxisCoordinate">{formatEventId(slice.startEventId)}</SliceStartCoordinate>}
                <SliceEndCoordinate data-testid="sliceAxisCoordinate">{formatEventId(boundaryEventId)}</SliceEndCoordinate>
            </SliceBlock>;
        })}
    </OverviewTrack>;
    return <div ref={rootRef}>
        <CompactOverview ref={compactRef}>
            {renderTrack()}
            <ExpandButton className="overview-expand-button" aria-label={t('overviewExpand')} title={t('overviewExpand')}
                aria-haspopup="dialog" aria-expanded={expanded} onClick={(event): void => openPopup(event.currentTarget)}>
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" aria-hidden="true">
                    <path d="M14 4h6v6M20 4l-7 7M10 20H4v-6M4 20l7-7" />
                </svg>
            </ExpandButton>
        </CompactOverview>
        {popupCreated && <OverviewDialog ref={dialogRef} aria-label={t('overviewExpandedTitle')}
            onPointerDown={(event): void => {
                const bounds = event.currentTarget.getBoundingClientRect();
                backdropPressRef.current = event.target === event.currentTarget &&
                    (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom);
            }}
            onClick={(event): void => {
                const bounds = event.currentTarget.getBoundingClientRect();
                if (backdropPressRef.current && event.target === event.currentTarget &&
                    (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom)) closePopup();
                backdropPressRef.current = false;
            }}
            onCancel={(event): void => { event.preventDefault(); closePopup(); }}>
            <CloseButton onClick={closePopup} aria-label={t('overviewClose')} title={t('overviewClose')}>
                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" aria-hidden="true">
                    <path d="m6 6 12 12M18 6 6 18" />
                </svg>
            </CloseButton>
            <Navigator ref={navigatorRef} data-testid="overviewNavigator" style={{ paddingTop: verticalLabels ? 46 : 20 }}>
                <OverviewCurve points={EMPTY_POINTS} index={overviewIndex} start={timelineStart} end={timelineEnd}
                    min={0} max={Math.max(1, globalMaxSize * 1.08)} height={48} nativeCanvas measureReady={dialogReady}>
                    {globalPeaks.map(({ point, index }) => renderPeak(point, index, timelineStart, timelineEnd, 0, valueMax, 48))}
                    {timelineEnd > timelineStart && <SliceRangeSelector minTime={timelineStart} maxTime={timelineEnd}
                        selectedRange={range} onRangeChange={updatePreviewRange} immediate />}
                </OverviewCurve>
                {deviceSlices.slices.slice(1).map(slice => <WindowBoundary key={slice.index} data-testid="overviewWindowBoundary"
                    style={{ left: `${(slice.startEventId - timelineStart) / timelineSpan * 100}%` }} />)}
                {deviceSlices.slices.filter((slice, index) => slice.index === popupSliceIndex ||
                    (index % labelStride === 0 && Math.abs(slice.index - popupSliceIndex) >= labelStride)).map(slice =>
                    <NavigatorLabel key={slice.index} data-testid="navigatorWindowLabel" data-vertical={verticalLabels}
                        title={`${t('snapshotWindow')} ${slice.index + 1}`} style={{
                            left: `${(slice.startEventId - timelineStart) / (timelineSpan + 1) * 100}%`,
                        }}>{verticalLabels ? slice.index + 1 : `${t('snapshotWindow')} ${slice.index + 1}`}</NavigatorLabel>)}
                <SliceStartCoordinate>{formatEventId(timelineStart)}</SliceStartCoordinate>
                <SliceEndCoordinate>{formatEventId(timelineEnd)}</SliceEndCoordinate>
            </Navigator>
            <OverviewSplitPane visible={tableVisible} onVisibleChange={setTableVisible} chart={
                <OverviewCanvas ref={canvasRef} tabIndex={0} aria-label={t('overviewChart')}
                    onPointerDown={beginChartDrag} onPointerMove={moveChartDrag}
                    onPointerUp={endChartDrag} onPointerCancel={endChartDrag}
                    onAuxClick={(event): void => { if (event.button === 1) event.preventDefault(); }}
                    onLostPointerCapture={(): void => { chartDragRef.current = undefined; setDragging(false); }}
                    data-testid="overviewCanvas" data-dragging={dragging}>
                    {globalPeaks.length > 0 && <PeakLegend data-testid="overviewPeakLegend">
                        <svg width="12" height="12" viewBox="0 0 12 12" fill="currentColor" aria-hidden="true"><path d="M6 1 11 6 6 11 1 6Z" /></svg>
                        {t('overviewGlobalPeakPoint')}
                    </PeakLegend>}
                    <OverviewZoomControls zoom={timelineSpan / rangeSpan}
                        activity={view.zoomActivity}
                        onZoom={(direction): void => scale(direction, xZoomMode)} onReset={resetView} />
                    <TrendSurface data-testid="expandedSliceOverview" data-range-start={range[0]} data-range-end={range[1]} data-value-min={previewMin} data-value-max={previewMax} style={{ overflow: 'hidden' }}>
                        <OverviewCurve points={EMPTY_POINTS} index={overviewIndex} start={range[0]} end={range[1]}
                            min={previewMin} max={previewMax} height={continuousHeight} nativeCanvas measureReady={dialogReady}>
                            {globalPeaks.filter(({ point }) => point.timestamp >= range[0] && point.timestamp <= range[1] &&
                                point.totalSize >= previewMin && point.totalSize <= previewMax).map(({ point, index }) =>
                                renderPeak(point, index, range[0], range[1], previewMin, previewMax, continuousHeight))}
                        </OverviewCurve>
                        <SliceStartCoordinate>{formatEventId(range[0])}</SliceStartCoordinate>
                        <SliceEndCoordinate>{formatEventId(range[1])}</SliceEndCoordinate>
                    </TrendSurface>
                </OverviewCanvas>
            } table={
                <RankingPanel>
                    <PeakRanking>
                        {rankingTable}
                    </PeakRanking>

                </RankingPanel>
            } />
        </OverviewDialog>}
    </div>;
};

export default MemSnapshotSliceOverview;
