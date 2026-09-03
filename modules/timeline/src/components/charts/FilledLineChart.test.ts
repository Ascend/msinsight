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
import { findFilledLineValueRange } from './filledLineUtils';

describe('filled line series mode', () => {
    const data = [
        [0, 10, 20],
        [1, 30, 5],
    ];

    it('uses the accumulated value for existing stacked counters', () => {
        expect(findFilledLineValueRange(data, 'stacked')).toEqual([0, 42]);
    });

    it('uses each independent value for an overlay counter', () => {
        expect(findFilledLineValueRange(data, 'overlay')).toEqual([0, 36]);
    });
});
