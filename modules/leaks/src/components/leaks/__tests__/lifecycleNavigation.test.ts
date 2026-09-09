/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import { isObservable, observable } from 'mobx';
import {
    calculateLifecyclePanTransform,
    calculateLifecycleZoomTransform,
    constrainLifecycleRange,
    getLifecycleZoomLimits,
    getLifecycleMarkerBaseline,
    isEditableKeyboardTarget,
    isLifecycleHostZoomShortcut,
    memoryValueToScreenY,
    resolveLifecycleKeyboardAction,
    screenYToMemoryValue,
} from '../lifecycleNavigation';

describe('lifecycleNavigation', () => {
    it('returns plain Worker payloads when an observable transform reaches a zoom boundary', () => {
        const transform = observable({ x: 0, y: 0, scaleX: 1, scaleY: 1 });
        const options = { transform, viewport: { width: 1000, height: 500 }, anchorX: 500, anchorY: 250,
            direction: 1 as const, onlyScaleX: false, limits: { scaleX: 1, scaleY: 1 } };
        for (const result of [calculateLifecycleZoomTransform(options),
            calculateLifecycleZoomTransform({ ...options, anchorX: NaN })]) {
            expect(result).toEqual(transform);
            expect(isObservable(result)).toBe(false);
        }
    });

    it('keeps the anchor stable after thousands of zoom events and allows reverse zoom', () => {
        const viewport = { width: 1000, height: 500 };
        const limits = getLifecycleZoomLimits(viewport, { x: 0.01, y: 0.01, offset: 0 });
        for (const onlyScaleX of [true, false]) {
            let transform = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
            for (let step = 0; step < 2000; step++) {
                transform = calculateLifecycleZoomTransform({
                    transform, viewport, limits, anchorX: 500, anchorY: 250, direction: 1, onlyScaleX,
                });
            }
            expect(transform.scaleX).toBe(limits.scaleX);
            expect(transform.scaleY).toBeLessThanOrEqual(limits.scaleY);
            expect(500 * transform.scaleX + transform.x).toBeCloseTo(500);
            expect(250 * transform.scaleY + transform.y).toBeCloseTo(250);
            expect(Math.abs(Math.fround(Math.fround(500 * Math.fround(transform.scaleX)) + Math.fround(transform.x)) - 500))
                .toBeLessThan(0.25);
            expect(calculateLifecycleZoomTransform({
                transform, viewport, limits, anchorX: 500, anchorY: 250, direction: 1, onlyScaleX,
            })).toEqual(transform);
            expect(calculateLifecycleZoomTransform({
                transform, viewport, limits, anchorX: 500, anchorY: 250, direction: -1, onlyScaleX,
            }).scaleX).toBeLessThan(transform.scaleX);
        }
    });

    it('accounts for large data origins and viewport dimensions in precision limits', () => {
        const viewport = { width: 1000, height: 500 };
        const normal = getLifecycleZoomLimits(viewport, { x: 1, y: 1, offset: 0 });
        const largeOrigin = getLifecycleZoomLimits(viewport, { x: 1, y: 1, offset: 10000 }, 10000);
        expect(largeOrigin.scaleX).toBeLessThan(normal.scaleX);
        expect(largeOrigin.scaleY).toBeLessThan(normal.scaleY);
        expect(getLifecycleZoomLimits(viewport, { x: Infinity, y: NaN, offset: 1 })).toEqual({ scaleX: 1, scaleY: 1 });
        expect(getLifecycleZoomLimits(viewport, { x: 500, y: 500, offset: 0 })).toEqual({ scaleX: 2, scaleY: 1 });
    });

    it('expands tiny overview selections inside the domain and preserves ordinary ranges', () => {
        expect(constrainLifecycleRange([499, 501], [0, 1000], 10)).toEqual([450, 550]);
        expect(constrainLifecycleRange([0, 1], [0, 1000], 10)).toEqual([0, 100]);
        expect(constrainLifecycleRange([999, 1000], [0, 1000], 10)).toEqual([900, 1000]);
        expect(constrainLifecycleRange([200, 400], [0, 1000], 10)).toEqual([200, 400]);
        expect(constrainLifecycleRange([1, 1], [0, 1000], 10)).toBeNull();
        expect(constrainLifecycleRange([1, Infinity], [0, 1000], 10)).toBeNull();
        expect(constrainLifecycleRange([0, 0.001], [0, 1000], 10000)).toEqual([0, 1]);
    });

    it('leaves a valid view unchanged for invalid zoom inputs', () => {
        const transform = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
        const options = { transform, viewport: { width: 1000, height: 500 }, anchorX: 500, anchorY: 250,
            direction: 1 as const, onlyScaleX: false };
        expect(calculateLifecycleZoomTransform({ ...options, anchorX: NaN })).toEqual(transform);
        expect(calculateLifecycleZoomTransform({ ...options, viewport: { width: 0, height: 500 } })).toEqual(transform);
    });

    it('resolves Timeline-aligned shortcuts for lifecycle graphs', () => {
        expect(resolveLifecycleKeyboardAction({ key: 'w' })).toBe('zoom-x-in');
        expect(resolveLifecycleKeyboardAction({ key: 'S', ctrlKey: true })).toBe('zoom-all-out');
        expect(resolveLifecycleKeyboardAction({ key: 'ArrowLeft' })).toBe('pan-left');
        expect(resolveLifecycleKeyboardAction({ key: 'ArrowUp' })).toBe('pan-up');
        expect(resolveLifecycleKeyboardAction({ key: 'k' }, true)).toBe('add-marker');
        expect(resolveLifecycleKeyboardAction({ key: 'k', repeat: true }, true)).toBeNull();
        expect(resolveLifecycleKeyboardAction({ key: 'k' })).toBeNull();
        expect(resolveLifecycleKeyboardAction({ key: 'k' }, false)).toBeNull();
        expect(resolveLifecycleKeyboardAction({ key: 'w' }, false)).toBe('zoom-x-in');
        expect(resolveLifecycleKeyboardAction({ key: 'w', isComposing: true })).toBeNull();
    });

    it('uses the hovered block lower-left path point as the marker baseline', () => {
        expect(getLifecycleMarkerBaseline({ path: [[10, 256], [20, 768]] })).toBe(256);
        expect(getLifecycleMarkerBaseline({ path: [] })).toBeNull();
        expect(getLifecycleMarkerBaseline(undefined)).toBeNull();
    });

    it('projects stable memory values after vertical zoom and pan', () => {
        const options = {
            minSize: 0,
            maxSize: 1_000,
            transformY: -100,
            scaleY: 2,
            zoomY: 0.2,
            viewportHeight: 200,
        };
        expect(screenYToMemoryValue(200, options)).toBe(250);
        expect(screenYToMemoryValue(100, options)).toBe(500);
        expect(screenYToMemoryValue(0, options)).toBe(750);
        expect(memoryValueToScreenY(500, options)).toBe(100);
        expect(screenYToMemoryValue(-1, options)).toBeNull();
    });

    it('reserves host close and save shortcuts only for an active lifecycle graph', () => {
        expect(isLifecycleHostZoomShortcut({ key: 'w', ctrlKey: true }, true)).toBe(true);
        expect(isLifecycleHostZoomShortcut({ key: 'S', metaKey: true }, true)).toBe(true);
        expect(isLifecycleHostZoomShortcut({ key: 'w', ctrlKey: true }, false)).toBe(false);
        expect(isLifecycleHostZoomShortcut({ key: 'w', ctrlKey: true, shiftKey: true }, true)).toBe(false);
    });

    it('keeps the zoom anchor stable and changes only requested axes', () => {
        const transform = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
        const viewport = { width: 200, height: 100 };
        const horizontalZoom = calculateLifecycleZoomTransform({
            transform, viewport, anchorX: 50, anchorY: 50, direction: 1, onlyScaleX: true,
        });
        const proportionalZoom = calculateLifecycleZoomTransform({
            transform, viewport, anchorX: 50, anchorY: 50, direction: 1, onlyScaleX: false,
        });
        expect(horizontalZoom.x).toBeCloseTo(-5);
        expect(horizontalZoom).toMatchObject({ y: 0, scaleX: 1.1, scaleY: 1 });
        expect(proportionalZoom.scaleY).toBeGreaterThan(1);
        expect(proportionalZoom.y).toBeLessThan(0);
    });

    it('clamps keyboard and pointer panning to the viewport bounds', () => {
        const transform = { x: 0, y: 0, scaleX: 2, scaleY: 2 };
        expect(calculateLifecyclePanTransform(transform, { width: 200, height: 100 }, 1_000, -1_000))
            .toMatchObject({ x: 200, y: -200 });
    });

    it('ignores editable targets and unsupported modifier combinations', () => {
        expect(isEditableKeyboardTarget(document.createElement('input'))).toBe(true);
        expect(isEditableKeyboardTarget(document.createElement('textarea'))).toBe(true);
        expect(isEditableKeyboardTarget(document.createElement('canvas'))).toBe(false);
        expect(resolveLifecycleKeyboardAction({ key: 'ArrowUp', shiftKey: true })).toBeNull();
        expect(resolveLifecycleKeyboardAction({ key: 'w', altKey: true })).toBeNull();
    });
});
