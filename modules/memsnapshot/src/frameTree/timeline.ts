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
import { getSnapshotEvent } from '../api/requests';

export interface EventProjection {
    minEventId: number;
    maxEventId: number;
    width: number;
    zoomX: number;
    scaleX: number;
    transformX: number;
}
const validProjection = (p: EventProjection): boolean =>
    Object.values(p).every(Number.isFinite) && p.width > 0 && p.zoomX > 0 && p.scaleX > 0 && p.maxEventId >= p.minEventId;

export function screenXToEventCoordinate(x: number, projection: EventProjection): number | null {
    if (!validProjection(projection) || !Number.isFinite(x) || x < 0 || x > projection.width) return null;
    const value = projection.minEventId + (x - projection.transformX) / (projection.zoomX * projection.scaleX);
    return value >= projection.minEventId && value <= projection.maxEventId ? value : null;
}

export function eventCoordinateToX(eventId: number, projection: EventProjection): number | null {
    if (!validProjection(projection) || !Number.isSafeInteger(eventId) ||
        eventId < projection.minEventId || eventId > projection.maxEventId) return null;
    const x = (eventId - projection.minEventId) * projection.zoomX * projection.scaleX + projection.transformX;
    return x >= 0 && x <= projection.width ? x : null;
}

export async function resolveFrameTreeEvent(coordinate: number, deviceId: string, sliceIndex: number): Promise<number | null> {
    const eventId = Math.floor(coordinate);
    if (!deviceId || !Number.isSafeInteger(eventId) || eventId < 0) return null;
    const page = await getSnapshotEvent({
        deviceId,
        sliceIndex,
        isTable: true,
        currentPage: 1,
        pageSize: 1,
        orderBy: 'id',
        desc: true,
        rangeFilters: { id: [0, eventId] },
    });
    const candidate = page.events?.[0]?.id;
    return Number.isSafeInteger(candidate) && candidate >= 0 && candidate <= eventId ? candidate : null;
}
