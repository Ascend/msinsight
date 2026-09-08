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

import type { InsightUnit } from '../../../entity/insight';
import type { Session } from '../../../entity/session';
import { getOffsetSide, getTimeOffset, OFFSET_SIDE } from '../offset';

describe('getOffsetSide for DPU', () => {
    it('uses the host offset for DPU units', () => {
        expect(getOffsetSide('DPU')).toBe(OFFSET_SIDE.HOST);
    });

    it('reads the DPU time offset from the host category', () => {
        const metadata = { cardId: 'rank0', metaType: 'DPU' };
        const unit = { metadata: { cardId: 'rank0' } } as unknown as InsightUnit;
        const session = {
            units: [unit],
            unitsConfig: {
                offsetConfig: {
                    timestampOffset: {
                        rank0__host: 100,
                        rank0__device: 200,
                    },
                },
            },
        } as unknown as Session;

        expect(getTimeOffset(session, metadata)).toBe(100);
    });

    it('keeps unknown metadata types on the host offset', () => {
        expect(getOffsetSide('UNKNOWN_META_TYPE')).toBe(OFFSET_SIDE.HOST);
    });
});
