/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    applyMemSnapshotEventLocate,
    getPreferredMemSnapshotDeviceSlices,
    resolveMemSnapshotSliceIndexByEventId,
} from '../memSnapshotSlices';
import type { MemSnapshotDeviceSliceInfo } from '@/entity/session';

const deviceSlices: MemSnapshotDeviceSliceInfo = {
    eventCount: 200,
    sliceCount: 2,
    readySlices: [0, 1],
    slices: [
        { index: 0, startEventId: 0, endEventId: 99, ready: true },
        { index: 1, startEventId: 100, endEventId: 199, ready: true },
    ],
};

describe('memSnapshotSlices', () => {
    it('resolves the ready slice that owns an event id', () => {
        expect(resolveMemSnapshotSliceIndexByEventId(deviceSlices, 0)).toBe(0);
        expect(resolveMemSnapshotSliceIndexByEventId(deviceSlices, 150)).toBe(1);
        expect(resolveMemSnapshotSliceIndexByEventId(deviceSlices, -1)).toBeUndefined();
        expect(resolveMemSnapshotSliceIndexByEventId(deviceSlices, 200)).toBeUndefined();
        expect(resolveMemSnapshotSliceIndexByEventId(undefined, 10)).toBeUndefined();
    });

    it('prefers the current device slices and falls back to the first map entry', () => {
        const otherSlices = { ...deviceSlices, eventCount: 1 };
        const snapshotSlices = { 0: deviceSlices, 1: otherSlices };
        expect(getPreferredMemSnapshotDeviceSlices(snapshotSlices, '0')).toBe(deviceSlices);
        expect(getPreferredMemSnapshotDeviceSlices(snapshotSlices, '1')).toBe(otherSlices);
        expect(getPreferredMemSnapshotDeviceSlices({}, '0')).toBeUndefined();
        expect(getPreferredMemSnapshotDeviceSlices(snapshotSlices, 'missing')).toBe(
            snapshotSlices[Object.keys(snapshotSlices)[0]],
        );
    });

    it('switches the selected slice before locating a cross-window event', () => {
        const session = {
            deviceId: '0',
            selectedSliceIndex: 1,
            snapshotSlices: { 0: deviceSlices },
            pendingEventLocate: null as { eventId: number; deviceId: string } | null,
        };
        applyMemSnapshotEventLocate(session, 10, '0');
        expect(session.selectedSliceIndex).toBe(0);
        expect(session.pendingEventLocate).toEqual({ eventId: 10, deviceId: '0' });
    });
});
