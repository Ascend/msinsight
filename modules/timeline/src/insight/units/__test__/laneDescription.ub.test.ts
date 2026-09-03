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
import { getLaneDescriptionKey } from '../laneDescription';

describe('UB counter lane description', () => {
    it('matches an A5 UB port lane', () => {
        const unit = {
            name: 'Counter',
            metadata: { processId: 'UB', processName: 'UB', threadName: 'UB Port004', metaType: 'UB' },
        } as unknown as InsightUnit;
        expect(getLaneDescriptionKey(unit)).toBe('laneDescriptions.npuMetrics.ubUdmaPorts');
    });
});
