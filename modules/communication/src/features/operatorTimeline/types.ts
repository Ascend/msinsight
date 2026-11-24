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
import type { Operator, RankTimeline, Timeline } from '../../domain/types';
import type { Range } from './navigation';

/** Slow-rank navigation uses the fastest rank's rounded time window, in milliseconds. */
export interface Highlight { rankId: string; name: string; startValue: number; endValue: number }
export interface OperatorHit { row: RankTimeline; op: Operator }
export interface TimelineProps {
    data: Timeline;
    dark: boolean;
    highlight?: Highlight;
    canLocate: boolean;
    onLocate: (row: RankTimeline, op: Operator) => void;
    onAlign: (name: string) => void;
    aligned: string;
    initialRange?: Range;
    onRangeChange?: (range: Range) => void;
}
