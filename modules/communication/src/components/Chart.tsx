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
import React from 'react';
import type { EChartsOption } from 'echarts';
import type { ChartsHandle } from '@insight/lib/components/MIChart';
import { MIChart } from '@insight/lib/components';
/** Keep all chart lifecycle, resizing, theme and default palette in the system component. */
export const Chart = React.forwardRef<ChartsHandle, { option: EChartsOption; dark?: boolean; height?: number; label: string; onEvents?: React.ComponentProps<typeof MIChart>['onEvents'] }>(function Chart({ option, height = 340, label, onEvents }, ref): JSX.Element {
    return <div role="img" aria-label={label}><MIChart ref={ref} options={option} onEvents={onEvents} height={`${height}px`} /></div>;
});
