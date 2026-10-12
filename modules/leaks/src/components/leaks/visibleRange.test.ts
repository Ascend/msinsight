/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { getInitialZoomDomain } from './zoomDomain';
import type { InitialZoomDomainInput } from './zoomDomain';
import {
    computeVisibleRange,
    hasLoadedGraphDomain,
    invertVisibleXRange,
    type VisibleRangeView,
} from './visibleRange';

const domain = (
    blockMin: number,
    blockMax: number,
    allocationMin = blockMin,
    allocationMax = blockMax,
    funcMin = 0,
    funcMax = 0,
): InitialZoomDomainInput => ({
    blockMinTimestamp: blockMin,
    blockMaxTimestamp: blockMax,
    allocationMinTimestamp: allocationMin,
    allocationMaxTimestamp: allocationMax,
    funcMinTimestamp: funcMin,
    funcMaxTimestamp: funcMax,
});

const viewForRange = (min: number, max: number): VisibleRangeView => ({
    viewport: { width: max - min },
    zoom: { x: 1, offset: min },
    transform: { x: 0, scaleX: 1 },
});

const defaultEmptyDomain = domain(0, 0, 0, 0, 0, 0);

test('inverts the current canvas window from x-axis transform only', () => {
    expect(invertVisibleXRange({
        viewport: { width: 200 },
        zoom: { x: 2, offset: 50 },
        transform: { x: -40, scaleX: 4 },
    })).toEqual({ min: 55, max: 80 });
});

test('rejects non-positive or non-finite invert inputs instead of using the full domain', () => {
    expect(invertVisibleXRange(viewForRange(10, 10))).toBeNull();
    expect(invertVisibleXRange({
        viewport: { width: 0 },
        zoom: { x: 1, offset: 0 },
        transform: { x: 0, scaleX: 1 },
    })).toBeNull();
    expect(invertVisibleXRange({
        viewport: { width: 100 },
        zoom: { x: 0, offset: 0 },
        transform: { x: 0, scaleX: 1 },
    })).toBeNull();
    expect(invertVisibleXRange({
        viewport: { width: 100 },
        zoom: { x: 1, offset: 0 },
        transform: { x: 0, scaleX: 0 },
    })).toBeNull();
    expect(invertVisibleXRange({
        viewport: { width: Number.NaN },
        zoom: { x: 1, offset: 0 },
        transform: { x: 0, scaleX: 1 },
    })).toBeNull();
    expect(invertVisibleXRange({
        viewport: { width: Number.MAX_VALUE },
        zoom: { x: Number.MIN_VALUE, offset: 0 },
        transform: { x: 0, scaleX: Number.MIN_VALUE },
    })).toBeNull();
});

test('treats the default empty 0-0 domain as unloaded', () => {
    expect(hasLoadedGraphDomain({
        blockMinTimestamp: 0,
        blockMaxTimestamp: 0,
        allocationCount: 0,
        funcTraceCount: 0,
        progressiveTotalEventCount: 0,
    })).toBe(false);
    expect(computeVisibleRange(viewForRange(0, 100), defaultEmptyDomain, 'timestamp', false)).toEqual({
        status: 'unavailable',
        visibleRange: null,
    });
});

test('does not treat allocation or func counts as loaded while sizeInfo is still the default 0-0', () => {
    expect(hasLoadedGraphDomain({
        blockMinTimestamp: 0,
        blockMaxTimestamp: 0,
        allocationCount: 1,
        funcTraceCount: 0,
        progressiveTotalEventCount: 0,
    })).toBe(false);
    expect(hasLoadedGraphDomain({
        blockMinTimestamp: 0,
        blockMaxTimestamp: 0,
        allocationCount: 0,
        funcTraceCount: 1,
        progressiveTotalEventCount: 0,
    })).toBe(false);
});

test('does not treat a single-point block domain as loaded without records', () => {
    expect(hasLoadedGraphDomain({
        blockMinTimestamp: 42,
        blockMaxTimestamp: 42,
        allocationCount: 0,
        funcTraceCount: 0,
        progressiveTotalEventCount: 0,
    })).toBe(false);
});

test('treats a strict block domain as loaded without record counts', () => {
    expect(hasLoadedGraphDomain({
        blockMinTimestamp: 0,
        blockMaxTimestamp: 100,
        allocationCount: 0,
        funcTraceCount: 0,
        progressiveTotalEventCount: 0,
    })).toBe(true);
});

test('treats progressiveTotalEventCount as sufficient loaded evidence', () => {
    expect(hasLoadedGraphDomain({
        blockMinTimestamp: 0,
        blockMaxTimestamp: 0,
        allocationCount: 0,
        funcTraceCount: 0,
        progressiveTotalEventCount: 1,
    })).toBe(true);
    expect(computeVisibleRange(viewForRange(0, 100), defaultEmptyDomain, 'timestamp', true)).toEqual({
        status: 'ready',
        visibleRange: { min: 0, max: 0 },
    });
});

test('accepts a loaded single-point domain when the window covers that point', () => {
    expect(hasLoadedGraphDomain({
        blockMinTimestamp: 42,
        blockMaxTimestamp: 42,
        allocationCount: 1,
        funcTraceCount: 0,
        progressiveTotalEventCount: 0,
    })).toBe(true);
    expect(computeVisibleRange(viewForRange(40, 50), domain(42, 42, 42, 42), 'timestamp', true)).toEqual({
        status: 'ready',
        visibleRange: { min: 42, max: 42 },
    });
});

test('returns the loaded domain for the default full window', () => {
    expect(computeVisibleRange(viewForRange(0, 100), domain(0, 100), 'timestamp', true)).toEqual({
        status: 'ready',
        visibleRange: { min: 0, max: 100 },
    });
});

test('keeps timestamp precision and does not apply display scaling', () => {
    expect(computeVisibleRange(
        viewForRange(1000.25, 2000.75),
        domain(0, 3000),
        'timestamp',
        true,
    )).toEqual({
        status: 'ready',
        visibleRange: { min: 1000.25, max: 2000.75 },
    });
});

test('ceils and floors snapshot event bounds without expanding the window', () => {
    expect(computeVisibleRange(viewForRange(10.2, 20.8), domain(0, 100), 'eventId', true)).toEqual({
        status: 'ready',
        visibleRange: { min: 11, max: 20 },
    });
    expect(computeVisibleRange(viewForRange(10, 20), domain(0, 100), 'eventId', true)).toEqual({
        status: 'ready',
        visibleRange: { min: 10, max: 20 },
    });
    expect(computeVisibleRange(viewForRange(10.2, 11.2), domain(0, 100), 'eventId', true)).toEqual({
        status: 'ready',
        visibleRange: { min: 11, max: 11 },
    });
    expect(computeVisibleRange(viewForRange(10.2, 10.8), domain(0, 100), 'eventId', true)).toEqual({
        status: 'empty',
        visibleRange: null,
    });
});

test('intersects overflow instead of shifting a preserved-width window', () => {
    const overflowView = viewForRange(-20, 20);
    const loadedDomain = domain(0, 100);
    expect(computeVisibleRange(overflowView, loadedDomain, 'timestamp', true)).toEqual({
        status: 'ready',
        visibleRange: { min: 0, max: 20 },
    });
    expect(computeVisibleRange(overflowView, loadedDomain, 'eventId', true)).toEqual({
        status: 'ready',
        visibleRange: { min: 0, max: 20 },
    });
    expect(computeVisibleRange(viewForRange(110, 120), loadedDomain, 'timestamp', true)).toEqual({
        status: 'empty',
        visibleRange: null,
    });
});

test('reuses getInitialZoomDomain when allocation or func metadata widens the axis', () => {
    const combined = domain(10, 20, 0, 50, 5, 80);
    expect(getInitialZoomDomain(combined)).toEqual({ minTime: 5, maxTime: 80 });
    expect(computeVisibleRange(viewForRange(0, 100), combined, 'timestamp', true)).toEqual({
        status: 'ready',
        visibleRange: { min: 5, max: 80 },
    });
});

test('returns unavailable for non-finite domain bounds or unsafe event ids', () => {
    expect(computeVisibleRange(viewForRange(0, 10), domain(Number.NaN, 10), 'timestamp', true)).toEqual({
        status: 'unavailable',
        visibleRange: null,
    });
    expect(computeVisibleRange(
        viewForRange(Number.MAX_SAFE_INTEGER + 1, Number.MAX_SAFE_INTEGER + 20),
        domain(0, Number.MAX_VALUE),
        'eventId',
        true,
    )).toEqual({
        status: 'unavailable',
        visibleRange: null,
    });
});
