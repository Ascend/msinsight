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
import { eventCoordinateToX, screenXToEventCoordinate, resolveFrameTreeEvent } from '../frameTree/timeline';
import { getSnapshotEvent } from '../api/requests';
jest.mock('../api/requests', () => ({ getSnapshotEvent: jest.fn() }));

const projection = { minEventId: 100, maxEventId: 200, width: 1000, zoomX: 10, scaleX: 2, transformX: -200 };
it('uses the real event-axis projection rather than pixels or relative indices', () => {
    expect(screenXToEventCoordinate(500, projection)).toBe(135);
    expect(eventCoordinateToX(135, projection)).toBe(500);
    expect(eventCoordinateToX(90, projection)).toBeNull();
    expect(screenXToEventCoordinate(-1, projection)).toBeNull();
    expect(eventCoordinateToX(135, { ...projection, zoomX: 0 })).toBeNull();
});
it('resolves the nearest preceding actual event through the existing event query', async () => {
    (getSnapshotEvent as jest.Mock).mockResolvedValue({ total: 1, events: [{ id: 132 }] });
    expect(await resolveFrameTreeEvent(135.8, '0', 2)).toBe(132);
    expect(getSnapshotEvent).toHaveBeenCalledWith(expect.objectContaining({
        deviceId: '0', sliceIndex: 2, rangeFilters: { id: [0, 135] }, desc: true, currentPage: 1, pageSize: 1,
    }));
});
it('does not send unsafe or negative event IDs and rejects non-preceding results', async () => {
    expect(await resolveFrameTreeEvent(Number.MAX_SAFE_INTEGER + 1, '0', 2)).toBeNull();
    expect(getSnapshotEvent).not.toHaveBeenCalled();
    (getSnapshotEvent as jest.Mock).mockResolvedValue({ total: 1, events: [{ id: 200 }] });
    expect(await resolveFrameTreeEvent(135, '0', 2)).toBeNull();
});
