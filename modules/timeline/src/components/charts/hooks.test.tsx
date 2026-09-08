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

import { act, renderHook } from '@testing-library/react';
import type { MapFunc, StatusData } from '../../entity/chart';
import type { InsightUnit } from '../../entity/insight';
import type { Session } from '../../entity/session';
import { useData } from './hooks';

const createSession = (): Session => ({
    phase: 'download',
    domainRange: { domainStart: 0, domainEnd: 100 },
    endTimeAll: 100,
    units: [],
    unitsConfig: {
        filterConfig: { pythonFunction: false },
        offsetConfig: { timestampOffset: {} },
    },
    autoAdjustUnitHeight: false,
    areFlagEventsHidden: false,
    alignRender: false,
    threadsToFetch: new Map(),
} as unknown as Session);

const createUnit = (): InsightUnit => ({
    name: 'Label',
    metadata: { cardId: 'rank0', metaType: 'DPU' },
    phase: 'download',
} as unknown as InsightUnit);

describe('useData', () => {
    it('redraws full-trace summary data without refetching it after a domain change', async () => {
        const session = createSession();
        const mapFunc = jest.fn().mockResolvedValue([]) as MapFunc<'status'>;
        const processor = jest.fn((data: StatusData[]): StatusData[] => data);
        const unit = createUnit();
        const { rerender } = renderHook(() => useData({
            session,
            mapFunc,
            unit,
            metadata: unit.metadata,
            width: 100,
            processor,
            refetchOnDomainChange: false,
        }));

        await act(async () => {
            await Promise.resolve();
        });
        expect(mapFunc).toHaveBeenCalledTimes(1);
        expect(processor).toHaveBeenCalledWith([], 100, 0, 100);

        session.domainRange = { domainStart: 20, domainEnd: 80 };
        rerender();
        await act(async () => {
            await Promise.resolve();
        });

        expect(mapFunc).toHaveBeenCalledTimes(1);
    });

    it('keeps domain refetching enabled by default for viewport-scoped charts', async () => {
        const session = createSession();
        const mapFunc = jest.fn().mockResolvedValue([]) as MapFunc<'status'>;
        const unit = createUnit();
        const { rerender } = renderHook(() => useData({
            session,
            mapFunc,
            unit,
            metadata: unit.metadata,
            width: 100,
        }));

        await act(async () => {
            await Promise.resolve();
        });
        expect(mapFunc).toHaveBeenCalledTimes(1);

        session.domainRange = { domainStart: 20, domainEnd: 80 };
        rerender();
        await act(async () => {
            await Promise.resolve();
        });

        expect(mapFunc).toHaveBeenCalledTimes(2);
    });
});
