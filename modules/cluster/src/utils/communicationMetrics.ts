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
export type CommunicationMetric = number | null;

export const isCommunicationMetric = (value: unknown, isDifference = false): value is number => typeof value === 'number' && Number.isFinite(value) && (isDifference || value >= 0);

export const formatCommunicationMetric = (value: unknown, isDifference = false): number | '-' => isCommunicationMetric(value, isDifference) ? value : '-';

export const compareCommunicationMetrics = (left: CommunicationMetric, right: CommunicationMetric, isDifference = false): number => {
    if (!isCommunicationMetric(left, isDifference)) {
        return isCommunicationMetric(right, isDifference) ? -1 : 0;
    }
    if (!isCommunicationMetric(right, isDifference)) {
        return 1;
    }
    return left - right;
};
