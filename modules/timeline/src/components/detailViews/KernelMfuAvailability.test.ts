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

import type { InsightUnit } from '../../entity/insight';
import { containsThreadingAnalysisUnit } from './KernelMfuAvailability';

jest.mock('../../api/request', () => ({
    queryKernelMfuAvailability: jest.fn(),
}));

const unit = (metaType: string, children: InsightUnit[] = []): InsightUnit => ({
    metadata: { metaType },
    children,
} as unknown as InsightUnit);

describe('Kernel MFU optional Threading probe', () => {
    it('detects Threading Analysis only from an explicit metadata node', () => {
        expect(containsThreadingAnalysisUnit([unit('HOST', [unit('THREADING_ANALYSIS')])])).toBe(true);
        expect(containsThreadingAnalysisUnit([unit('HOST'), unit('NPU')])).toBe(false);
    });
});
