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

import {
    getColorStringByAddr,
    getColorStringByIndex,
    getDimmedColorStringByAddr,
    getDimmedColorStringByIndex,
    GL_COLORS,
    hashHexAddressToIndex,
    normalizeColorIndex,
} from './color';

describe('block color', () => {
    it('uses the same normal color for an address and its color index', () => {
        const address = '0x1020';
        const colorIndex = hashHexAddressToIndex(address);

        expect(getColorStringByAddr(address)).toBe(getColorStringByIndex(colorIndex));
    });

    it('keeps normal and dimmed colors as separate rendering states', () => {
        const address = '0x1020';

        expect(getDimmedColorStringByAddr(address)).not.toBe(getColorStringByAddr(address));
        expect(GL_COLORS).toHaveLength(10);
    });

    it('normalizes invalid and out-of-range color indexes', () => {
        expect(normalizeColorIndex(-1)).toBe(GL_COLORS.length - 1);
        expect(normalizeColorIndex(GL_COLORS.length + 1)).toBe(1);
        expect(normalizeColorIndex(Number.NaN)).toBe(0);
        expect(getColorStringByIndex(-1)).toBe(getColorStringByIndex(GL_COLORS.length - 1));
        expect(getDimmedColorStringByIndex(-1)).toBe(getDimmedColorStringByIndex(GL_COLORS.length - 1));
    });
});
