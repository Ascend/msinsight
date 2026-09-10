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
import type { MemSnapshotDeviceSliceInfo } from '@/entity/session';

export const resolveMemSnapshotSliceIndexByEventId = (
    deviceSlices: MemSnapshotDeviceSliceInfo | undefined,
    eventId: number,
): number | undefined => {
    if (deviceSlices === undefined || !Number.isFinite(eventId) || eventId < 0) {
        return undefined;
    }
    return deviceSlices.slices.find(slice =>
        slice.ready && eventId >= slice.startEventId && eventId <= slice.endEventId)?.index;
};

export const getPreferredMemSnapshotDeviceSlices = (
    snapshotSlices: Record<string, MemSnapshotDeviceSliceInfo | undefined>,
    deviceId: string,
): MemSnapshotDeviceSliceInfo | undefined => {
    if (deviceId !== '' && snapshotSlices[deviceId] !== undefined) {
        return snapshotSlices[deviceId];
    }
    const firstDeviceId = Object.keys(snapshotSlices)[0];
    return firstDeviceId === undefined ? undefined : snapshotSlices[firstDeviceId];
};

export const applyMemSnapshotEventLocate = (
    session: {
        deviceId: string;
        selectedSliceIndex: number;
        snapshotSlices: Record<string, MemSnapshotDeviceSliceInfo | undefined>;
        pendingEventLocate: { eventId: number; deviceId: string } | null;
    },
    eventId: number,
    deviceId: string,
): void => {
    const targetSliceIndex = resolveMemSnapshotSliceIndexByEventId(session.snapshotSlices[deviceId], eventId);
    if (targetSliceIndex !== undefined && targetSliceIndex !== session.selectedSliceIndex) {
        session.selectedSliceIndex = targetSliceIndex;
    }
    session.pendingEventLocate = { eventId, deviceId };
};
