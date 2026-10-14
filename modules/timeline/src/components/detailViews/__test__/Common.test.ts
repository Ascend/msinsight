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

        expect(getNames(visibleItems)).toEqual([KERNEL_MFU_VIEW_NAME, ...ftraceTypes]);
        expect(visibleItems.map(item => item.originIndex)).toEqual(
            [KERNEL_MFU_VIEW_NAME, ...ftraceTypes].map(name => statsSystemViewItems.findIndex(item => item.name === name)),
        );
    });

    it('shows non-ftrace labels and dynamic layers when only non-ftrace data has been imported', () => {
        const visibleItems = getVisibleStatsSystemViewItems(itemsWithDynamicLayer, false, true);

        expect(getNames(visibleItems)).toEqual([
            'Overall Metrics',
            'Python API Summary',
            'Python API Trace',
            'CANN API Summary',
            'CANN API Trace',
            'Ascend HardWare Task Summary',
            'Ascend HardWare Task Trace',
            'Communication Summary',
            'Communication Trace',
            'Overlap Analysis',
            'Kernel Dispatch Duration Breakdown',
            'Kernel Details',
            'Kernel Summary',
            'Memcpy Overall',
            KERNEL_MFU_VIEW_NAME,
            'Custom Layer',
        ]);
    });

    it('shows all labels after ftrace and non-ftrace data are both imported', () => {
        const visibleItems = getVisibleStatsSystemViewItems(itemsWithDynamicLayer, true, true);

        expect(getNames(visibleItems)).toEqual(getNames(itemsWithDynamicLayer));
    });
});
