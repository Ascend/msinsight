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
    addLifecycleMemoryMarker,
    deleteLifecycleMemoryMarker,
    getLifecycleMemoryMarkerColor,
    getLifecycleMemoryMarkerContextKey,
    getLifecycleMemoryMarkerOrdinal,
} from '../lifecycleMemoryMarkers';
import { Session } from '../session';

describe('lifecycleMemoryMarkers', () => {
    it('isolates marker collections by analysis context', () => {
        const base = { fileHash: 'file', module: 'memsnapshot', deviceId: '0', eventType: 'malloc' };
        expect(getLifecycleMemoryMarkerContextKey(base)).not.toBe(
            getLifecycleMemoryMarkerContextKey({ ...base, deviceId: '1' }),
        );
    });

    it('adds, sorts, de-duplicates, and deletes markers', () => {
        let markers = addLifecycleMemoryMarker([], 300.4, 'upper');
        markers = addLifecycleMemoryMarker(markers, 100.2, 'lower');
        expect(markers.map(marker => marker.memoryBytes)).toEqual([100, 300]);
        expect(markers.map(marker => marker.ordinal)).toEqual([2, 1]);
        expect(markers.map(marker => marker.color)).toEqual(['#8C8C8C', '#8C8C8C']);
        expect(addLifecycleMemoryMarker(markers, 100, 'duplicate')).toBe(markers);

        expect(deleteLifecycleMemoryMarker(markers, 'lower')).toEqual([{
            id: 'upper',
            memoryBytes: 300,
            color: '#8C8C8C',
            ordinal: 1,
        }]);
    });

    it('supports a non-reused ordinal and derives stable legacy marker presentation', () => {
        const markers = addLifecycleMemoryMarker([], 128, 'third', undefined, 3);
        expect(markers[0]).toMatchObject({ ordinal: 3, color: '#8C8C8C' });
        expect(getLifecycleMemoryMarkerOrdinal({ id: 'legacy', memoryBytes: 64 }, 1)).toBe(2);
        expect(getLifecycleMemoryMarkerColor({ id: 'legacy', memoryBytes: 64 }, 1)).toBe('#8C8C8C');
    });

    it('isolates session markers and clears them during session cleanup', () => {
        const session = new Session();
        session.fileHash = 'snapshot';
        session.module = 'memsnapshot';
        session.eventType = 'malloc';
        session.deviceId = '0';
        session.addLifecycleMemoryMarker(128, 'device-0');
        session.deviceId = '1';
        session.addLifecycleMemoryMarker(256, 'device-1');
        session.deviceId = '0';
        expect(session.getLifecycleMemoryMarkers().map(marker => marker.id)).toEqual(['device-0']);
        session.clearLifecycleMemoryMarkers();
        expect(session.getLifecycleMemoryMarkers()).toEqual([]);
    });
});
