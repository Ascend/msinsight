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

export type FilledLineSeriesMode = 'stacked' | 'overlay';

export const findFilledLineValueRange = (
    dataList: number[][], seriesMode: FilledLineSeriesMode = 'stacked',
): [number, number] => {
    if (dataList.length === 0) { return [0, 0]; }
    let minHeight = 0;
    let maxHeight = 0;
    dataList.forEach(data => {
        if (seriesMode === 'overlay') {
            data.slice(1).forEach(value => {
                const height = isNaN(Number(value)) ? 0 : Number(value);
                minHeight = Math.min(minHeight, height);
                maxHeight = Math.max(maxHeight, height);
            });
            return;
        }
        let height = 0;
        data.forEach((value, index) => {
            if (index === 0) { return; }
            height += isNaN(Number(value)) ? 0 : Number(value);
        });
        minHeight = Math.min(minHeight, height);
        maxHeight = Math.max(maxHeight, height);
    });
    return [minHeight, maxHeight * 1.2];
};
