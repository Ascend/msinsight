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

const VIRTUAL_SCROLL_THRESHOLD = 30;

/** Heights include the existing 1px border, and are rebuilt only when observed layout inputs change. */
export function buildUnitOffsets(units: ReadonlyArray<{ height: () => number }>): number[] {
    const offsets = [0];
    for (const unit of units) {
        offsets.push(offsets[offsets.length - 1] + unit.height() + 1);
    }
    return offsets;
}

export function computeVisibleRange(offsets: readonly number[], viewportHeight: number, scrollTop: number): [number, number] {
    const count = offsets.length - 1;
    let left = 0;
    let right = count;
    // Preserve the strict boundary comparisons and extra trailing lane of computeVisibleUnitRange.
    while (left < right) {
        const middle = Math.floor((left + right) / 2);
        if (offsets[middle] + VIRTUAL_SCROLL_THRESHOLD <= viewportHeight + scrollTop) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    const end = left;
    left = 0;
    right = end;
    while (left < right) {
        const middle = Math.floor((left + right) / 2);
        if (offsets[middle + 1] - 1 + VIRTUAL_SCROLL_THRESHOLD < scrollTop) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return [left, end + 1];
}
