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
import type { Session } from '../../entity/session';
import { getBottomPanelUnit, getSelectedDataUnit } from '../selectionContext';

const createUnit = (hasRenderer = true): InsightUnit => ({
    bottomPanelRender: hasRenderer ? jest.fn(() => []) : undefined,
} as unknown as InsightUnit);

describe('selection context', () => {
    it('keeps the selected slice source separate from the range-selected lanes', () => {
        const firstRangeUnit = createUnit();
        const selectedDataUnit = createUnit();
        const session = {
            selectedUnits: [firstRangeUnit, selectedDataUnit],
            selectedDataUnit,
        } as unknown as Session;

        expect(getSelectedDataUnit(session)).toBe(selectedDataUnit);
        expect(getBottomPanelUnit(session, true)).toBe(selectedDataUnit);
        expect(getBottomPanelUnit(session, false)).toBe(firstRangeUnit);
    });

    it('falls back to the range selection for legacy selection paths', () => {
        const firstRangeUnit = createUnit();
        const session = { selectedUnits: [firstRangeUnit] } as unknown as Session;

        expect(getSelectedDataUnit(session)).toBe(firstRangeUnit);
        expect(getBottomPanelUnit(session, true)).toBe(firstRangeUnit);
    });

    it('does not mix another lane renderer with a known source that cannot render details', () => {
        const firstRangeUnit = createUnit();
        const selectedDataUnit = createUnit(false);
        const session = {
            selectedUnits: [firstRangeUnit, selectedDataUnit],
            selectedDataUnit,
        } as unknown as Session;

        expect(getBottomPanelUnit(session, true)).toBeUndefined();
    });
});
