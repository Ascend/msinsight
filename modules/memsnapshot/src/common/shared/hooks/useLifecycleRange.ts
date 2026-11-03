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
import { useEffect, useRef, useState, type MutableRefObject, type Dispatch, type SetStateAction } from 'react';
import { runInAction } from 'mobx';
import { debounce, type DebouncedFunc } from 'lodash';
import type { MemoryContext } from '../state/MemoryContext';
import { getBlockRenderer } from '../../leaksWorker/blockWorker/worker';
import { constrainLifecycleRange, getLifecycleZoomLimits } from '../../components/leaks/lifecycleNavigation';
type TransformChangeSource = 'wheel' | 'keyboard' | 'drag';
const isValidRange = (range: [number, number]): boolean => Number.isFinite(range[0]) && Number.isFinite(range[1]) && range[0] < range[1];
const isSameRange = (prev: [number, number] | undefined, next: [number, number]): boolean => {
    if (prev === undefined) {
        return false;
    }
    return prev[0] === next[0] && prev[1] === next[1];
};
const clampRangeToBounds = (range: [number, number], min: number, max: number): [number, number] => {
    const totalRange = max - min;
    const rangeWidth = Math.min(range[1] - range[0], totalRange);
    let start = Math.max(min, Math.min(max - rangeWidth, range[0]));
    if (!Number.isFinite(start)) {
        start = min;
    }
    return [Math.floor(start), Math.floor(start + rangeWidth)];
};

interface RangeOptions {
    synchronizeSlider?: boolean;
    onRangeRequest?: (range: [number, number], shouldApply: () => boolean) => void;
    onRangeCommit?: () => void;
}
interface LifecycleRangeController {
    selectedRange: [number, number] | undefined;
    setSelectedRange: Dispatch<SetStateAction<[number, number] | undefined>>;
    selectedRangeRef: MutableRefObject<[number, number] | undefined>;
    zoomRangeRef: MutableRefObject<{ min: number; max: number }>;
    debouncedRangeRequestRef: MutableRefObject<DebouncedFunc<(range: [number, number]) => void> | null>;
    debouncedCommitRangeRef: MutableRefObject<DebouncedFunc<(range: [number, number]) => void> | null>;
    rangeRequestSeqRef: MutableRefObject<number>;
    commitRangeSideEffects: (range: [number, number]) => void;
    getCurrentRange: () => [number, number];
    requestRangeData: (range: [number, number]) => void;
    selectedZoomChange: (range: [number, number]) => void;
    syncDataZoomRange: (transform: RenderOptions['transform'], source?: TransformChangeSource) => void;
}
export const useLifecycleRange = (session: MemoryContext, options: RangeOptions): LifecycleRangeController => {
    const { transform: workerTransform } = getBlockRenderer(session);

    const currentSession = useRef(session);
    currentSession.current = session;
    const callbacks = useRef(options);
    callbacks.current = options;
    const [selectedRange, setSelectedRange] = useState<[number, number] | undefined>(undefined);
    const selectedRangeRef = useRef<[number, number] | undefined>(undefined);
    const zoomRangeRef = useRef({ min: Number.MAX_SAFE_INTEGER, max: Number.MIN_SAFE_INTEGER });
    const debouncedRangeRequestRef = useRef<DebouncedFunc<(range: [number, number]) => void> | null>(null);
    const debouncedCommitRangeRef = useRef<DebouncedFunc<(range: [number, number]) => void> | null>(null);
    const rangeRequestSeqRef = useRef(0);

    const commitSessionRange = (range: [number, number]): void => {
        runInAction(() => {
            currentSession.current.minTime = range[0];
            currentSession.current.maxTime = range[1];
        });
    };

    const commitRangeSideEffects = (range: [number, number]): void => {
        commitSessionRange(range);
        callbacks.current.onRangeCommit?.();
    };

    const getCurrentRange = (): [number, number] => {
        return selectedRangeRef.current ?? [session.minTime, session.maxTime];
    };

    const requestRangeData = (range: [number, number]): void => {
        if (!isValidRange(range)) {
            return;
        }
        const requestSeq = ++rangeRequestSeqRef.current;
        callbacks.current.onRangeRequest?.(range, () => requestSeq === rangeRequestSeqRef.current);
    };

    const scheduleRangeChange = (range: [number, number], syncDataZoom: boolean = true): void => {
        if (!isValidRange(range)) {
            return;
        }
        if (isSameRange(selectedRangeRef.current, range)) {
            return;
        }
        selectedRangeRef.current = range;
        setSelectedRange(syncDataZoom ? range : undefined);
        debouncedRangeRequestRef.current?.cancel();
        debouncedRangeRequestRef.current?.(range);
        debouncedCommitRangeRef.current?.cancel();
        debouncedCommitRangeRef.current?.(range);
    };

    if (debouncedRangeRequestRef.current === null) {
        debouncedRangeRequestRef.current = debounce((range: [number, number]): void => {
            requestRangeData(range);
        }, 500);
    }
    if (debouncedCommitRangeRef.current === null) {
        debouncedCommitRangeRef.current = debounce((range: [number, number]): void => {
            commitRangeSideEffects(range);
        }, 300);
    }

    const selectedZoomChange = (range: [number, number]): void => {
        if (!isValidRange(range)) {
            return;
        }
        const { sizeInfo, renderOptions } = session.leaksWorkerInfo;
        const limits = getLifecycleZoomLimits(renderOptions.viewport, renderOptions.zoom, sizeInfo.minSize);
        const actualRange = constrainLifecycleRange(range, [sizeInfo.minTimestamp, sizeInfo.maxTimestamp], limits.scaleX);
        if (actualRange === null || !isValidRange(actualRange) || renderOptions.zoom.x <= 0 ||
            !Number.isFinite(renderOptions.zoom.x)) {
            return;
        }
        // A DOM slider needs its selection to stay synchronized with the rendered range.
        // Reflect an expanded range in the overview and dependent queries as well.
        const adjusted = !isSameRange(range, actualRange);
        scheduleRangeChange(actualRange, adjusted || callbacks.current.synchronizeSlider === true);
        if (adjusted) {
            // A repeated selection can hit the same limit after the slider moved.
            setSelectedRange(actualRange);
        }
        const newScale = (sizeInfo.maxTimestamp - sizeInfo.minTimestamp) / (actualRange[1] - actualRange[0]);
        const newX = -(actualRange[0] - sizeInfo.minTimestamp) * renderOptions.zoom.x * newScale;
        const transform = { x: newX, y: 0, scaleX: newScale, scaleY: 1 };

        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
    };

    const syncDataZoomRange = (transform: RenderOptions['transform'], _source: TransformChangeSource = 'wheel'): void => {
        const { renderOptions } = session.leaksWorkerInfo;
        const { scaleX } = transform;
        const { viewport, zoom } = renderOptions;
        if (scaleX <= 0 || zoom.x <= 0 || viewport.width <= 0) {
            return;
        }
        const { min, max } = zoomRangeRef.current;
        if (min >= max) {
            return;
        }
        const visibleRange = viewport.width / zoom.x / scaleX;
        const visibleMinTime = zoom.offset - transform.x / scaleX / zoom.x;
        const range = clampRangeToBounds([visibleMinTime, visibleMinTime + visibleRange], min, max);
        if (!isValidRange(range)) {
            return;
        }
        scheduleRangeChange(range);
    };

    useEffect(() => () => {
        rangeRequestSeqRef.current++;
        debouncedRangeRequestRef.current?.cancel();
        debouncedCommitRangeRef.current?.cancel();
    }, [session]);
    return {
        selectedRange,
        setSelectedRange,
        selectedRangeRef,
        zoomRangeRef,
        debouncedRangeRequestRef,
        debouncedCommitRangeRef,
        rangeRequestSeqRef,
        commitRangeSideEffects,
        getCurrentRange,
        requestRangeData,
        selectedZoomChange,
        syncDataZoomRange,
    };
};
