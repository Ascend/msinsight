/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
    ftraceTypes,
    getVisibleStatsSystemViewItems,
    statsSystemViewItems,
    KERNEL_MFU_VIEW_NAME,
    type SystemViewItem,
} from '../Common';

const getNames = (items: SystemViewItem[]): string[] => items.map(item => item.name);

describe('getVisibleStatsSystemViewItems', () => {
    const dynamicLayer = { name: 'Custom Layer', description: 'custom layer description' };
    const itemsWithDynamicLayer = [...statsSystemViewItems, dynamicLayer];

    it('shows ftrace and MFU labels when only ftrace data has been imported', () => {
        const visibleItems = getVisibleStatsSystemViewItems(itemsWithDynamicLayer, true, false);
        const expected = itemsWithDynamicLayer.filter(item =>
            item.name === KERNEL_MFU_VIEW_NAME || ftraceTypes.includes(item.name));

        expect(getNames(visibleItems)).toEqual(getNames(expected));
        expect(visibleItems.map(item => item.originIndex)).toEqual(
            expected.map(item => itemsWithDynamicLayer.indexOf(item)),
        );
    });

    it('shows non-ftrace labels and dynamic layers when only non-ftrace data has been imported', () => {
        const visibleItems = getVisibleStatsSystemViewItems(itemsWithDynamicLayer, false, true);
        const expected = itemsWithDynamicLayer.filter(item => !ftraceTypes.includes(item.name));

        expect(getNames(visibleItems)).toEqual(getNames(expected));
        expect(visibleItems.map(item => item.originIndex)).toEqual(
            expected.map(item => itemsWithDynamicLayer.indexOf(item)),
        );
    });

    it('shows all labels after ftrace and non-ftrace data are both imported', () => {
        const visibleItems = getVisibleStatsSystemViewItems(itemsWithDynamicLayer, true, true);

        expect(getNames(visibleItems)).toEqual(getNames(itemsWithDynamicLayer));
    });
});
