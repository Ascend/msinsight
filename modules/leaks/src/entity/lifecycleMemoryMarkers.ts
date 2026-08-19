/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export type LifecycleMemoryMarkerSource = 'custom' | 'block';

export interface LifecycleMemoryMarker {
    id: string;
    memoryBytes: number;
    color?: string;
    ordinal?: number;
    source?: LifecycleMemoryMarkerSource;
    blockId?: number;
}

export const DEFAULT_LIFECYCLE_MEMORY_MARKER_COLOR = '#8C8C8C';
export const LIFECYCLE_MEMORY_MARKER_COLORS = [
    '#4C7DFF',
    '#FF7A45',
    '#2FA87C',
    '#9A6DE2',
    '#D6A319',
    '#E45C91',
    '#2F9EAA',
    '#7A86D8',
];

export interface LifecycleMemoryMarkerContext {
    fileHash: string;
    module: string;
    deviceId: string;
    eventType: string;
}

const normalizeMemoryBytes = (memoryBytes: number): number => Math.round(memoryBytes);

export const getLifecycleMemoryMarkerContextKey = (context: LifecycleMemoryMarkerContext): string =>
    JSON.stringify([context.fileHash, context.module, context.deviceId, context.eventType]);

export const sortLifecycleMemoryMarkers = (markers: LifecycleMemoryMarker[]): LifecycleMemoryMarker[] =>
    [...markers].sort((left, right) => left.memoryBytes - right.memoryBytes || left.id.localeCompare(right.id));

export const getLifecycleMemoryMarkerOrdinal = (
    marker: LifecycleMemoryMarker,
    fallbackIndex: number,
): number => {
    const ordinal = marker.ordinal ?? 0;
    return Number.isInteger(ordinal) && ordinal > 0 ? ordinal : fallbackIndex + 1;
};

export const getLifecycleMemoryMarkerColor = (
    marker: LifecycleMemoryMarker,
    fallbackIndex: number,
): string => marker.color ?? (getLifecycleMemoryMarkerSource(marker) === 'custom'
    ? DEFAULT_LIFECYCLE_MEMORY_MARKER_COLOR
    : LIFECYCLE_MEMORY_MARKER_COLORS[
        (getLifecycleMemoryMarkerOrdinal(marker, fallbackIndex) - 1) % LIFECYCLE_MEMORY_MARKER_COLORS.length
    ]);

export const getLifecycleMemoryMarkerSource = (
    marker: LifecycleMemoryMarker,
): LifecycleMemoryMarkerSource => marker.source ?? 'custom';

export const findLifecycleBlockMarkerAtMemory = (
    markers: LifecycleMemoryMarker[],
    memoryBytes: number,
    blockId?: number,
): LifecycleMemoryMarker | null => {
    if (!Number.isFinite(memoryBytes)) {
        return null;
    }
    const normalizedValue = normalizeMemoryBytes(memoryBytes);
    const blockMarkers = markers.filter(marker => getLifecycleMemoryMarkerSource(marker) === 'block');
    if (blockId !== undefined) {
        const exactMarker = blockMarkers.find(marker => marker.blockId === blockId);
        if (exactMarker !== undefined) {
            return exactMarker;
        }
        return blockMarkers.find(marker => marker.blockId === undefined && marker.memoryBytes === normalizedValue) ?? null;
    }
    return blockMarkers.find(marker => marker.memoryBytes === normalizedValue) ?? null;
};

const hasDuplicateLifecycleMemoryMarker = (
    markers: LifecycleMemoryMarker[],
    memoryBytes: number,
    source: LifecycleMemoryMarkerSource,
    blockId?: number,
): boolean => markers.some(marker => {
    if (getLifecycleMemoryMarkerSource(marker) !== source) {
        return false;
    }
    if (source === 'block' && blockId !== undefined && marker.blockId !== undefined) {
        return marker.blockId === blockId;
    }
    return marker.memoryBytes === memoryBytes;
});

export const addLifecycleMemoryMarker = (
    markers: LifecycleMemoryMarker[],
    memoryBytes: number,
    id: string,
    color?: string,
    requestedOrdinal?: number,
    source: LifecycleMemoryMarkerSource = 'custom',
    blockId?: number,
): LifecycleMemoryMarker[] => {
    if (!Number.isFinite(memoryBytes)) {
        return markers;
    }
    const normalizedValue = normalizeMemoryBytes(memoryBytes);
    if (hasDuplicateLifecycleMemoryMarker(markers, normalizedValue, source, blockId)) {
        return markers;
    }
    const sortedMarkers = sortLifecycleMemoryMarkers(markers);
    const normalizedMarkers = sortedMarkers.map((marker, index) => ({
        ...marker,
        ordinal: getLifecycleMemoryMarkerOrdinal(marker, index),
        color: getLifecycleMemoryMarkerColor(marker, index),
    }));
    const nextOrdinal = normalizedMarkers.reduce((maximum, marker) => Math.max(maximum, marker.ordinal), 0) + 1;
    const proposedOrdinal = requestedOrdinal ?? 0;
    const ordinal = Number.isInteger(proposedOrdinal) && proposedOrdinal >= nextOrdinal
        ? proposedOrdinal
        : nextOrdinal;
    const assignedColor = color ?? (source === 'custom'
        ? DEFAULT_LIFECYCLE_MEMORY_MARKER_COLOR
        : LIFECYCLE_MEMORY_MARKER_COLORS[(ordinal - 1) % LIFECYCLE_MEMORY_MARKER_COLORS.length]);
    return sortLifecycleMemoryMarkers([
        ...normalizedMarkers,
        {
            id,
            memoryBytes: normalizedValue,
            color: assignedColor,
            ordinal,
            source,
            ...(blockId === undefined ? {} : { blockId }),
        },
    ]);
};

export const deleteLifecycleMemoryMarker = (
    markers: LifecycleMemoryMarker[],
    id: string,
): LifecycleMemoryMarker[] => markers.filter(marker => marker.id !== id);
