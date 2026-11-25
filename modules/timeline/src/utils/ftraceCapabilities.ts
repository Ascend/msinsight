/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

interface FtraceCard {
    isFtrace?: boolean;
    ftraceCapabilities?: string[];
}

// Omission identifies legacy ftrace data. An explicit empty list advertises no derived statistics.
export const normalizeFtraceCapabilities = (value: unknown): string[] | undefined =>
    Array.isArray(value) ? value.filter((capability): capability is string => typeof capability === 'string') : undefined;

export const canShowFtraceTaskSummary = (card?: FtraceCard): boolean => card?.isFtrace === true && (
    card.ftraceCapabilities === undefined || card.ftraceCapabilities.some(capability =>
        ['scheduling', 'softirq', 'hardirq'].includes(capability),
    )
);

export const isFtraceSummaryColumnAvailable = (column: string, capabilities?: string[]): boolean => {
    if (capabilities === undefined || ['cpu', 'comm', 'pid'].includes(column)) {
        return true;
    }
    if (['running', 'context_switch_count'].includes(column)) {
        return capabilities.includes('scheduling');
    }
    if (['runnable', 'sleeping'].includes(column)) {
        return capabilities.includes('scheduling') && capabilities.includes('wakeup');
    }
    if (column.startsWith('soft_irq_')) {
        return capabilities.includes('softirq');
    }
    if (column.startsWith('hard_irq_')) {
        return capabilities.includes('hardirq');
    }
    return false;
};

export const getFtraceSummaryDefaultSorter = (capabilities?: string[]): { field: string; order: string } => {
    const field = ['running', 'soft_irq_duration', 'hard_irq_duration'].find(column =>
        isFtraceSummaryColumnAvailable(column, capabilities),
    ) ?? 'cpu';
    return { field, order: 'descend' };
};
