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

import {
    calculateLifecyclePanTransform,
    calculateLifecycleZoomTransform,
    isEditableKeyboardTarget,
    isLifecycleHostZoomShortcut,
    memoryValueToScreenY,
    resolveLifecycleKeyboardAction,
    screenYToMemoryValue,
} from '../lifecycleNavigation';

describe('lifecycleNavigation', () => {
    it('resolves Timeline-aligned shortcuts for lifecycle graphs', () => {
        expect(resolveLifecycleKeyboardAction({ key: 'w' })).toBe('zoom-x-in');
        expect(resolveLifecycleKeyboardAction({ key: 'S', ctrlKey: true })).toBe('zoom-all-out');
        expect(resolveLifecycleKeyboardAction({ key: 'ArrowLeft' })).toBe('pan-left');
        expect(resolveLifecycleKeyboardAction({ key: 'ArrowUp' })).toBe('pan-up');
        expect(resolveLifecycleKeyboardAction({ key: 'w', isComposing: true })).toBeNull();
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
