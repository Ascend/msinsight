/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { JsonObject } from '@insight/lib/FrontendAgentCommand';

const TOP_SEGMENT_LIMIT = 5;

interface SegmentSummary extends JsonObject {
    address: string;
    stream: number;
    capacityBytes: number;
    usedBytes: number;
    blockCount: number;
}

export interface MemoryPoolSummary extends JsonObject {
    eventId: number;
    segmentCount: number;
    blockCount: number;
    capacityBytes: number;
    usedBytes: number;
    freeBytes: number;
    overcommittedBytes: number;
    utilizationRatio: number;
    emptySegmentCount: number;
    lowUtilizationSegmentCount: number;
    largestSegments: SegmentSummary[];
}

export const summarizeMemoryPool = (eventId: number, segments: Segment[]): MemoryPoolSummary => {
    let capacityBytes = 0;
    let usedBytes = 0;
    let blockCount = 0;
    let emptySegmentCount = 0;
    let lowUtilizationSegmentCount = 0;
    const largestSegments: SegmentSummary[] = [];
    for (const segment of segments) {
        const segmentUsedBytes = segment.blocks.reduce((total, block) => total + finite(block.size), 0);
        const summary = {
            address: segment.address,
            stream: finite(segment.stream),
            capacityBytes: finite(segment.size),
            usedBytes: segmentUsedBytes,
            blockCount: segment.blocks.length,
        };
        capacityBytes += summary.capacityBytes;
        usedBytes += segmentUsedBytes;
        blockCount += summary.blockCount;
        if (summary.blockCount === 0) emptySegmentCount++;
        if (summary.capacityBytes > 0 && summary.usedBytes / summary.capacityBytes <= 0.25) lowUtilizationSegmentCount++;
        insertLargest(largestSegments, summary);
    }
    return {
        eventId,
        segmentCount: segments.length,
        blockCount,
        capacityBytes,
        usedBytes,
        freeBytes: Math.max(0, capacityBytes - usedBytes),
        overcommittedBytes: Math.max(0, usedBytes - capacityBytes),
        utilizationRatio: capacityBytes > 0 ? usedBytes / capacityBytes : 0,
        emptySegmentCount,
        lowUtilizationSegmentCount,
        largestSegments,
    };
};

const insertLargest = (items: SegmentSummary[], item: SegmentSummary): void => {
    const index = items.findIndex(current => item.capacityBytes > current.capacityBytes);
    if (index < 0) items.push(item);
    else items.splice(index, 0, item);
    if (items.length > TOP_SEGMENT_LIMIT) items.pop();
};

const finite = (value: unknown): number => Number.isFinite(Number(value)) ? Number(value) : 0;
