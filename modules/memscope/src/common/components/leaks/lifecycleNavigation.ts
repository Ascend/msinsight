/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export type LifecycleKeyboardAction =
    | 'zoom-x-in'
    | 'zoom-x-out'
    | 'zoom-all-in'
    | 'zoom-all-out'
    | 'pan-left'
    | 'pan-right'
    | 'pan-up'
    | 'pan-down'
    | 'add-marker';

export interface LifecycleKeyboardEventLike {
    key: string;
    ctrlKey?: boolean;
    metaKey?: boolean;
    altKey?: boolean;
    shiftKey?: boolean;
    repeat?: boolean;
    isComposing?: boolean;
}

export interface MemoryProjectionOptions {
    minSize: number;
    maxSize: number;
    transformY: number;
    scaleY: number;
    zoomY: number;
    viewportHeight: number;
}

export interface VisibleMemoryRange {
    min: number;
    max: number;
    span: number;
}

export interface LifecycleGraphTransform {
    x: number;
    y: number;
    scaleX: number;
    scaleY: number;
}

export interface LifecycleGraphViewport {
    width: number;
    height: number;
}

const clamp = (value: number, min: number, max: number): number => Math.min(Math.max(value, min), max);

export interface LifecycleZoomLimits {
    scaleX: number;
    scaleY: number;
}

// Float32 has 24 significant bits. Reserve rounding headroom for the shader's
// products and cancellation against translation, keeping pixel values below 2^20.
const MAX_LIFECYCLE_PIXEL_MAGNITUDE = 2 ** 20;

export const getLifecycleZoomLimits = (
    viewport: LifecycleGraphViewport,
    zoom: { x: number; y: number; offset: number },
    minSize = 0,
): LifecycleZoomLimits => {
    const limit = (extent: number): number => Number.isFinite(extent) && extent > 0
        ? Math.max(1, MAX_LIFECYCLE_PIXEL_MAGNITUDE / extent)
        : 1;
    // Linked time ranges use integer timestamps; memory coordinates use bytes.
    const resolutionLimit = (extent: number, zoomFactor: number): number =>
        Number.isFinite(zoomFactor) && zoomFactor > 0 && Number.isFinite(extent) && extent > 0
            ? Math.max(1, extent / zoomFactor)
            : 1;
    return {
        scaleX: Math.min(limit(Math.abs(zoom.offset * zoom.x) + viewport.width), resolutionLimit(viewport.width, zoom.x)),
        scaleY: Math.min(limit(Math.abs(minSize * zoom.y) + viewport.height), resolutionLimit(viewport.height, zoom.y)),
    };
};

export const constrainLifecycleRange = (
    range: [number, number],
    domain: [number, number],
    maxScale: number,
): [number, number] | null => {
    const total = domain[1] - domain[0];
    const requested = range[1] - range[0];
    if (![...range, ...domain, maxScale, total, requested].every(Number.isFinite) ||
        total <= 0 || requested <= 0 || maxScale < 1) {
        return null;
    }
    const span = Math.min(total, Math.max(requested, 1, total / maxScale));
    const start = clamp(range[0] - (span - requested) / 2, domain[0], domain[1] - span);
    return [start, start + span];
};

export const calculateLifecycleZoomTransform = ({
    transform,
    viewport,
    anchorX,
    anchorY,
    direction,
    onlyScaleX,
    zoomStep = 0.1,
    limits,
}: {
    transform: LifecycleGraphTransform;
    viewport: LifecycleGraphViewport;
    anchorX: number;
    anchorY: number;
    direction: 1 | -1;
    onlyScaleX: boolean;
    zoomStep?: number;
    limits?: LifecycleZoomLimits;
}): LifecycleGraphTransform => {
    if (![transform.x, transform.y, transform.scaleX, transform.scaleY,
        viewport.width, viewport.height, anchorX, anchorY, zoomStep].every(Number.isFinite) ||
        transform.scaleX <= 0 || transform.scaleY <= 0 || viewport.width <= 0 || viewport.height <= 0 || zoomStep <= 0) {
        return { ...transform };
    }
    const originalContentX = (anchorX - transform.x) / transform.scaleX;
    const originalContentY = (anchorY - transform.y) / transform.scaleY;
    const baseScale = onlyScaleX ? transform.scaleX : Math.max(transform.scaleX, transform.scaleY);
    let delta = direction * zoomStep * (Math.abs(baseScale - 1) + 1);
    if (limits !== undefined && direction > 0) {
        delta = Math.min(delta, Math.max(0, limits.scaleX - transform.scaleX),
            onlyScaleX ? Infinity : Math.max(0, limits.scaleY - transform.scaleY));
    }
    const scaleX = Math.max(0.1, transform.scaleX + delta);
    const scaleY = onlyScaleX ? transform.scaleY : Math.max(0.1, transform.scaleY + delta);
    if (scaleX === transform.scaleX && scaleY === transform.scaleY) {
        // Session transforms may be MobX proxies; Worker messages need plain data.
        return { ...transform };
    }

    return {
        x: clamp(anchorX - originalContentX * scaleX, -viewport.width * scaleX, viewport.width),
        y: onlyScaleX
            ? transform.y
            : clamp(anchorY - originalContentY * scaleY, -viewport.height * scaleY, viewport.height),
        scaleX,
        scaleY,
    };
};

export const calculateLifecyclePanTransform = (
    transform: LifecycleGraphTransform,
    viewport: LifecycleGraphViewport,
    deltaX: number,
    deltaY: number,
): LifecycleGraphTransform => ({
    ...transform,
    x: clamp(transform.x + deltaX, -viewport.width * transform.scaleX, viewport.width),
    y: clamp(transform.y + deltaY, -viewport.height * transform.scaleY, viewport.height),
});

export const isEditableKeyboardTarget = (target: EventTarget | null): boolean => {
    if (!(target instanceof HTMLElement)) {
        return false;
    }
    const tagName = target.tagName.toLowerCase();
    return target.isContentEditable || tagName === 'input' || tagName === 'textarea' || tagName === 'select';
};

export const isLifecycleHostZoomShortcut = (
    event: LifecycleKeyboardEventLike,
    graphInteractionActive: boolean,
): boolean => {
    if (
        !graphInteractionActive ||
        (event.isComposing ?? false) ||
        (event.altKey ?? false) ||
        (event.shiftKey ?? false) ||
        !((event.ctrlKey ?? false) || (event.metaKey ?? false))
    ) {
        return false;
    }
    const key = event.key.toLowerCase();
    return key === 'w' || key === 's';
};

export const resolveLifecycleKeyboardAction = (
    event: LifecycleKeyboardEventLike,
    markerShortcutEnabled = false,
): LifecycleKeyboardAction | null => {
    if ([event.isComposing, event.altKey, event.shiftKey].some(Boolean)) {
        return null;
    }

    const key = event.key.toLowerCase();
    const isCommandModifier = [event.ctrlKey, event.metaKey].some(Boolean);
    if (isCommandModifier) {
        if (key === 'w') {
            return 'zoom-all-in';
        }
        if (key === 's') {
            return 'zoom-all-out';
        }
        return null;
    }

    switch (key) {
        case 'w':
            return 'zoom-x-in';
        case 's':
            return 'zoom-x-out';
        case 'a':
        case 'arrowleft':
            return 'pan-left';
        case 'd':
        case 'arrowright':
            return 'pan-right';
        case 'arrowup':
            return 'pan-up';
        case 'arrowdown':
            return 'pan-down';
        case 'k':
            return markerShortcutEnabled && !event.repeat ? 'add-marker' : null;
        default:
            return null;
    }
};

export const getLifecycleMarkerBaseline = (
    block: Pick<Block, 'path'> | null | undefined,
): number | null => {
    const baseline = block?.path[0]?.[1];
    return baseline !== undefined && Number.isFinite(baseline) ? baseline : null;
};

export const getVisibleMemoryRange = (options: MemoryProjectionOptions): VisibleMemoryRange | null => {
    const { minSize, maxSize, transformY, scaleY, zoomY, viewportHeight } = options;
    if (
        ![minSize, maxSize, transformY, scaleY, zoomY, viewportHeight].every(Number.isFinite) ||
        maxSize <= minSize || scaleY <= 0 || zoomY <= 0 || viewportHeight <= 0
    ) {
        return null;
    }
    const span = viewportHeight / scaleY / zoomY;
    const min = minSize - transformY / scaleY / zoomY;
    return { min, max: min + span, span };
};

export const screenYToMemoryValue = (
    screenY: number,
    options: MemoryProjectionOptions,
): number | null => {
    if (!Number.isFinite(screenY) || screenY < 0 || screenY > options.viewportHeight) {
        return null;
    }
    const visibleRange = getVisibleMemoryRange(options);
    if (visibleRange === null) {
        return null;
    }
    const bottomOffset = options.viewportHeight - screenY;
    return visibleRange.min + (bottomOffset / options.viewportHeight) * visibleRange.span;
};

export const memoryValueToScreenY = (
    memoryBytes: number,
    options: MemoryProjectionOptions,
): number | null => {
    if (!Number.isFinite(memoryBytes)) {
        return null;
    }
    const visibleRange = getVisibleMemoryRange(options);
    if (visibleRange === null) {
        return null;
    }
    const bottomOffset = ((memoryBytes - visibleRange.min) / visibleRange.span) * options.viewportHeight;
    return options.viewportHeight - bottomOffset;
};
