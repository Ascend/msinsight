/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { getInitialZoomDomain, type InitialZoomDomain, type InitialZoomDomainInput } from './zoomDomain';

export type VisibleRangeAxis = 'timestamp' | 'eventId';
export type VisibleRangeStatus = 'ready' | 'unavailable' | 'empty';

export type ClosedRange = {
    min: number;
    max: number;
};

export type VisibleRangeComputation = {
    status: VisibleRangeStatus;
    visibleRange: ClosedRange | null;
};

export type VisibleRangeView = {
    viewport: { width: number };
    zoom: { x: number; offset: number };
    transform: { x: number; scaleX: number };
};

export type LoadedGraphDomainEvidence = {
    blockMinTimestamp: number;
    blockMaxTimestamp: number;
    allocationCount: number;
    funcTraceCount: number;
    progressiveTotalEventCount: number;
};

const isFiniteNumber = (value: number): boolean => Number.isFinite(value);

export const hasLoadedGraphDomain = (evidence: LoadedGraphDomainEvidence): boolean => {
    if (!isFiniteNumber(evidence.allocationCount) ||
        !isFiniteNumber(evidence.funcTraceCount) ||
        !isFiniteNumber(evidence.progressiveTotalEventCount)) {
        return false;
    }
    if (evidence.progressiveTotalEventCount > 0) {
        return true;
    }
    if (!isFiniteNumber(evidence.blockMinTimestamp) || !isFiniteNumber(evidence.blockMaxTimestamp)) {
        return false;
    }
    if (evidence.blockMaxTimestamp > evidence.blockMinTimestamp) {
        return true;
    }
    // Session default sizeInfo is 0-0 until worker dataInfo arrives. Allocation and
    // func records can land first; treating that window as loaded would invert the
    // default {x:1, offset:0} zoom against a data-space domain and return a false
    // empty or ready range. Keep allocation/func counts for non-default single-point
    // sizeInfo, which still cannot be distinguished from an accidental min===max
    // without those records.
    if (evidence.blockMinTimestamp === 0 && evidence.blockMaxTimestamp === 0) {
        return false;
    }
    return evidence.allocationCount > 0 || evidence.funcTraceCount > 0;
};

export const invertVisibleXRange = (view: VisibleRangeView): ClosedRange | null => {
    const width = view.viewport.width;
    const zoomX = view.zoom.x;
    const scaleX = view.transform.scaleX;
    const offset = view.zoom.offset;
    const translateX = view.transform.x;
    if (![width, zoomX, scaleX, offset, translateX].every(isFiniteNumber)) {
        return null;
    }
    if (width <= 0 || zoomX <= 0 || scaleX <= 0) {
        return null;
    }
    const rangeWidth = width / zoomX / scaleX;
    const rawMin = offset - translateX / scaleX / zoomX;
    const rawMax = rawMin + rangeWidth;
    if (!isFiniteNumber(rangeWidth) || !isFiniteNumber(rawMin) || !isFiniteNumber(rawMax) || rawMax <= rawMin) {
        return null;
    }
    return { min: rawMin, max: rawMax };
};

export const intersectVisibleRange = (
    raw: ClosedRange,
    domain: InitialZoomDomain,
): ClosedRange | 'empty' | null => {
    if (!isFiniteNumber(domain.minTime) || !isFiniteNumber(domain.maxTime)) {
        return null;
    }
    const min = Math.max(raw.min, domain.minTime);
    const max = Math.min(raw.max, domain.maxTime);
    if (!isFiniteNumber(min) || !isFiniteNumber(max)) {
        return null;
    }
    if (min > max) {
        return 'empty';
    }
    return { min, max };
};

export const normalizeVisibleRange = (
    range: ClosedRange,
    axis: VisibleRangeAxis,
): VisibleRangeComputation => {
    if (axis === 'timestamp') {
        if (!isFiniteNumber(range.min) || !isFiniteNumber(range.max)) {
            return { status: 'unavailable', visibleRange: null };
        }
        return { status: 'ready', visibleRange: { min: range.min, max: range.max } };
    }
    const min = Math.ceil(range.min);
    const max = Math.floor(range.max);
    if (!Number.isSafeInteger(min) || !Number.isSafeInteger(max)) {
        return { status: 'unavailable', visibleRange: null };
    }
    if (min > max) {
        return { status: 'empty', visibleRange: null };
    }
    return { status: 'ready', visibleRange: { min, max } };
};

export const computeVisibleRange = (
    view: VisibleRangeView,
    domainInput: InitialZoomDomainInput,
    axis: VisibleRangeAxis,
    loaded: boolean,
): VisibleRangeComputation => {
    if (!loaded) {
        return { status: 'unavailable', visibleRange: null };
    }
    const raw = invertVisibleXRange(view);
    if (raw === null) {
        return { status: 'unavailable', visibleRange: null };
    }
    const intersection = intersectVisibleRange(raw, getInitialZoomDomain(domainInput));
    if (intersection === null) {
        return { status: 'unavailable', visibleRange: null };
    }
    if (intersection === 'empty') {
        return { status: 'empty', visibleRange: null };
    }
    return normalizeVisibleRange(intersection, axis);
};
