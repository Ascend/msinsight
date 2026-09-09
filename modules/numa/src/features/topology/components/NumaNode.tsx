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
import type { NumaNode as NumaNodeData } from '@/entities/numa/types';
import { NUMA_HEIGHT, NUMA_WIDTH } from '../model/constants';
import type { Point } from '../model/types';

interface Props {
    numa: NumaNodeData;
    point: Point;
    selected: boolean;
    crossSocketValue?: string;
    crossSocketLabel: string;
    onSelect: () => void;
}

export const NumaNode: React.FC<Props> = ({
    numa,
    point,
    selected,
    crossSocketValue,
    crossSocketLabel,
    onSelect,
}) => (
    <g
        className={`numa-node ${selected ? 'selected' : ''}`}
        onClick={(event) => {
            event.stopPropagation();
            onSelect();
        }}
        data-testid={`numa-${numa.id}`}
    >
        <rect
            x={point.x - NUMA_WIDTH / 2}
            y={point.y - NUMA_HEIGHT / 2}
            width={NUMA_WIDTH}
            height={NUMA_HEIGHT}
            rx="12"
        />
        <text x={point.x} y={point.y - 10} textAnchor="middle" className="numa-title">{numa.name}</text>
        {crossSocketValue !== undefined && (
            <text x={point.x} textAnchor="middle" className="numa-value">
                <tspan x={point.x} y={point.y + 8} className="numa-value-label">
                    {crossSocketLabel}
                </tspan>
                <tspan x={point.x} y={point.y + 21} className="numa-value-text">
                    {crossSocketValue}
                </tspan>
            </text>
        )}
    </g>
);
