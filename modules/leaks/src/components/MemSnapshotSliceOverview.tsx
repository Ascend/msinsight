/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React, { useEffect, useMemo, useRef, useState } from 'react';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import type { MemSnapshotDeviceSliceInfo } from '@/entity/session';
import type { AllocationData } from '@/utils/RequestUtils';

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
        stroke: ${(props): string => props.theme.mode === 'dark' ? props.theme.primaryColor : '#516489'};
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

type DragMode = 'start' | 'end' | 'range';

const RANGE_CHANGE_DEBOUNCE_MS = 150;
const formatEventId = (eventId: number): string => String(Math.trunc(eventId));
const normalizeRange = (
    minTime: number,
    maxTime: number,
    selectedStart?: number,
    selectedEnd?: number,
): [number, number] => [
    Math.max(minTime, Math.min(maxTime, selectedStart ?? minTime)),
    Math.max(minTime, Math.min(maxTime, selectedEnd ?? maxTime)),
];

interface SliceRangeSelectorProps {
    minTime: number;
    maxTime: number;
    selectedRange?: [number, number];
    onRangeChange: (range: [number, number]) => void;
}

const SliceRangeSelector = ({
    minTime,
    maxTime,
    selectedRange,
    onRangeChange,
}: SliceRangeSelectorProps): React.ReactElement => {
    const trackRef = useRef<HTMLDivElement>(null);
    const dragRef = useRef<{
        mode: DragMode;
        originX: number;
        originRange: [number, number];
        trackWidth: number;
    } | undefined>(undefined);
    const rangeChangeTimerRef = useRef<number | undefined>(undefined);
    const onRangeChangeRef = useRef(onRangeChange);
    const timeSpan = Math.max(1, maxTime - minTime);
    const minRange = Math.max(1, Math.round(timeSpan / 100));
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
        setRange(normalizeRange(minTime, maxTime, selectedStart, selectedEnd));
    }, [minTime, maxTime, selectedStart, selectedEnd]);

    useEffect(() => (): void => {
        if (rangeChangeTimerRef.current !== undefined) {
            window.clearTimeout(rangeChangeTimerRef.current);
        }
    }, []);

    const scheduleRangeChange = (nextRange: [number, number]): void => {
        setRange(nextRange);
        if (rangeChangeTimerRef.current !== undefined) {
            window.clearTimeout(rangeChangeTimerRef.current);
        }
        rangeChangeTimerRef.current = window.setTimeout(() => {
            rangeChangeTimerRef.current = undefined;
            onRangeChangeRef.current(nextRange);
        }, RANGE_CHANGE_DEBOUNCE_MS);
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
            aria-label="Range start"
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
            aria-label="Range end"
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

const buildTrendPoints = (
    allocations: Array<[number, number]>,
    minTime: number,
    maxTime: number,
    maxSize: number,
): { line: string; area: string } => {
    const timeSpan = Math.max(1, maxTime - minTime);
    const sizeSpan = Math.max(1, maxSize);
    const points = allocations.map(([timestamp, totalSize]) => {
        const x = (timestamp - minTime) / timeSpan * 100;
        const y = 34 - totalSize / sizeSpan * 32;
        return [x, y] as const;
    });
    if (points.length === 0) {
        return { line: '', area: '' };
    }
    const line = points.map(([x, y]) => `${x},${y}`).join(' ');
    return {
        line,
        area: `${points[0][0]},36 ${line} ${points[points.length - 1][0]},36`,
    };
};

const MemSnapshotSliceOverview = ({
    deviceSlices,
    overviewData,
    selectedSliceIndex,
    selectedRange,
    onSelectSlice,
    onRangeChange,
}: Props): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const globalMaxSize = useMemo(() => {
        let maxSize = 1;
        Object.values(overviewData).forEach(data => {
            data.allocations.forEach(item => {
                maxSize = Math.max(maxSize, item.totalSize);
            });
        });
        return maxSize;
    }, [overviewData]);

    return <OverviewTrack data-testid="snapshotSliceOverview">
        {deviceSlices.slices.map((slice, index) => {
            const data = overviewData[slice.index];
            const isSelected = slice.index === selectedSliceIndex;
            const sliceEventCount = Math.max(1, slice.endEventId - slice.startEventId + 1);
            const dataSource = data?.allocations.map(item => [item.timestamp, item.totalSize] as [number, number]) ?? [];
            const trendPoints = buildTrendPoints(
                dataSource,
                slice.startEventId,
                slice.endEventId,
                globalMaxSize,
            );
            // 中间刻度取下一窗 startEventId（即当前窗 end+1），表示分窗边界而非末事件 ID。
            const boundaryEventId = deviceSlices.slices[index + 1]?.startEventId ?? slice.endEventId;
            return <SliceBlock
                key={slice.index}
                role="button"
                tabIndex={slice.ready ? 0 : -1}
                aria-disabled={!slice.ready}
                data-selected={String(isSelected)}
                data-ready={String(slice.ready)}
                style={{ flexGrow: sliceEventCount }}
                onClick={(): void => {
                    if (slice.ready) {
                        onSelectSlice(slice.index);
                    }
                }}
                onKeyDown={(event): void => {
                    if (slice.ready && (event.key === 'Enter' || event.key === ' ')) {
                        event.preventDefault();
                        onSelectSlice(slice.index);
                    }
                }}
            >
                <SliceLabel>{`${t('snapshotWindow')} ${slice.index + 1}${slice.ready ? '' : ` · ${t('pending')}`}`}</SliceLabel>
                {!slice.ready
                    ? <EmptySlice>{t('pending')}</EmptySlice>
                    : <SliceTrend>
                        <svg width="100%" height="36" viewBox="0 0 100 36" preserveAspectRatio="none" aria-hidden="true">
                            <polygon className="slice-trend-area" points={trendPoints.area} />
                            <polyline
                                className="slice-trend-line"
                                strokeWidth="1"
                                vectorEffect="non-scaling-stroke"
                                points={trendPoints.line}
                            />
                        </svg>
                        {isSelected
                            ? <SliceRangeSelector
                                minTime={slice.startEventId}
                                maxTime={slice.endEventId}
                                selectedRange={selectedRange}
                                onRangeChange={onRangeChange}
                            />
                            : <></>}
                    </SliceTrend>}
                {index === 0
                    ? <SliceStartCoordinate data-testid="sliceAxisCoordinate">
                        {formatEventId(slice.startEventId)}
                    </SliceStartCoordinate>
                    : <></>}
                <SliceEndCoordinate data-testid="sliceAxisCoordinate">
                    {formatEventId(boundaryEventId)}
                </SliceEndCoordinate>
            </SliceBlock>;
        })}
    </OverviewTrack>;
};

export default MemSnapshotSliceOverview;
