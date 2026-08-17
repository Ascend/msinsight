/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export interface LifecycleMemoryMarker {
    id: string;
    memoryBytes: number;
    color?: string;
    ordinal?: number;
}

export const DEFAULT_LIFECYCLE_MEMORY_MARKER_COLOR = '#8C8C8C';

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
    _fallbackIndex: number,
): string => marker.color ?? DEFAULT_LIFECYCLE_MEMORY_MARKER_COLOR;

export const addLifecycleMemoryMarker = (
    markers: LifecycleMemoryMarker[],
    memoryBytes: number,
    id: string,
    color?: string,
    requestedOrdinal?: number,
): LifecycleMemoryMarker[] => {
    if (!Number.isFinite(memoryBytes)) {
        return markers;
    }
    const normalizedValue = normalizeMemoryBytes(memoryBytes);
    if (markers.some(marker => marker.memoryBytes === normalizedValue)) {
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
    const assignedColor = color ?? DEFAULT_LIFECYCLE_MEMORY_MARKER_COLOR;
    return sortLifecycleMemoryMarkers([
        ...normalizedMarkers,
        {
            id,
            memoryBytes: normalizedValue,
            color: assignedColor,
            ordinal,
        },
    ]);
};

export const deleteLifecycleMemoryMarker = (
    markers: LifecycleMemoryMarker[],
    id: string,
): LifecycleMemoryMarker[] => markers.filter(marker => marker.id !== id);
