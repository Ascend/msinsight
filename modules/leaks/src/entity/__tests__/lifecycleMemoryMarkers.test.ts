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
    findLifecycleBlockMarkerAtMemory,
    getLifecycleMemoryMarkerColor,
    getLifecycleMemoryMarkerContextKey,
    getLifecycleMemoryMarkerLabel,
    getLifecycleMemoryMarkerOrdinal,
    getLifecycleMemoryMarkerSource,
    updateLifecycleMemoryMarkerColor,
    updateLifecycleMemoryMarkerPresentation,
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
            source: 'custom',
        }]);
    });

    it('supports a non-reused ordinal and derives stable legacy marker presentation', () => {
        const markers = addLifecycleMemoryMarker([], 128, 'third', undefined, 3);
        expect(markers[0]).toMatchObject({ ordinal: 3, color: '#8C8C8C' });
        expect(getLifecycleMemoryMarkerOrdinal({ id: 'legacy', memoryBytes: 64 }, 1)).toBe(2);
        expect(getLifecycleMemoryMarkerColor({ id: 'legacy', memoryBytes: 64 }, 1)).toBe('#8C8C8C');
        expect(getLifecycleMemoryMarkerSource({ id: 'legacy', memoryBytes: 64 })).toBe('custom');
        expect(addLifecycleMemoryMarker([], 256, 'block', undefined, 1, 'block', 42)[0]).toMatchObject({
            source: 'block',
            blockId: 42,
        });
    });

    it('finds block-source markers by block identity with a legacy baseline fallback', () => {
        const markers = [
            { id: 'custom', memoryBytes: 128, source: 'custom' as const },
            { id: 'block-1', memoryBytes: 256, source: 'block' as const, blockId: 1 },
            { id: 'block-2', memoryBytes: 256, source: 'block' as const, blockId: 2 },
            { id: 'legacy-block', memoryBytes: 512, source: 'block' as const },
        ];
        expect(findLifecycleBlockMarkerAtMemory(markers, 255.6, 2)?.id).toBe('block-2');
        expect(findLifecycleBlockMarkerAtMemory(markers, 512, 3)?.id).toBe('legacy-block');
        expect(findLifecycleBlockMarkerAtMemory(markers, 256, 3)).toBeNull();
        expect(findLifecycleBlockMarkerAtMemory(markers, 128)).toBeNull();
        expect(findLifecycleBlockMarkerAtMemory(markers, Number.NaN)).toBeNull();
    });

    it('de-duplicates markers by source and block identity', () => {
        let markers = addLifecycleMemoryMarker([], 128, 'custom', undefined, 1, 'custom');
        markers = addLifecycleMemoryMarker(markers, 128, 'block-1', undefined, 2, 'block', 1);
        markers = addLifecycleMemoryMarker(markers, 128, 'block-2', undefined, 3, 'block', 2);
        expect(markers.map(marker => marker.id)).toEqual(['block-1', 'block-2', 'custom']);
        expect(addLifecycleMemoryMarker(markers, 256, 'same-block', undefined, 4, 'block', 1)).toBe(markers);
        expect(addLifecycleMemoryMarker(markers, 128, 'same-custom', undefined, 4, 'custom')).toBe(markers);
    });

    it('updates marker colors without changing identity', () => {
        const markers = [
            { id: 'custom', memoryBytes: 128, source: 'custom' as const, ordinal: 1, color: '#8C8C8C' },
            { id: 'other', memoryBytes: 256, source: 'custom' as const, ordinal: 2, color: '#4C7DFF' },
        ];
        expect(updateLifecycleMemoryMarkerColor(markers, 'custom', '#00aa00')
            .find(marker => marker.id === 'custom')?.color).toBe('#00AA00');
        expect(updateLifecycleMemoryMarkerColor(markers, 'custom', 'invalid')).toBe(markers);
    });

    it('keeps stable identity while renaming and temporarily hiding a marker', () => {
        const markers = addLifecycleMemoryMarker([], 128, 'flag');
        const updated = updateLifecycleMemoryMarkerPresentation(markers, 'flag', {
            name: '  Peak memory  ',
            hidden: true,
        });
        expect(updated[0]).toMatchObject({ ordinal: 1, name: 'Peak memory', hidden: true });
        expect(getLifecycleMemoryMarkerLabel(updated[0], 0)).toBe('Peak memory');
        expect(updateLifecycleMemoryMarkerPresentation(updated, 'flag', { name: '' })[0].name).toBeUndefined();
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
        session.clearCurrentLifecycleMemoryMarkers();
        expect(session.getLifecycleMemoryMarkers()).toEqual([]);
        session.deviceId = '1';
        expect(session.getLifecycleMemoryMarkers().map(marker => marker.id)).toEqual(['device-1']);
        session.clearLifecycleMemoryMarkers();
        expect(session.getLifecycleMemoryMarkers()).toEqual([]);
    });
});
