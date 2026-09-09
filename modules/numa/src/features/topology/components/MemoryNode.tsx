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
import { MEMORY_HEIGHT, MEMORY_WIDTH } from '../model/constants';

interface Props {
    id: string;
    x: number;
    y: number;
    selected: boolean;
    onSelect?: () => void;
}

export const MemoryNode: React.FC<Props> = ({ id, x, y, selected, onSelect }) => (
    <g
        className={`memory-node ${selected ? 'selected' : ''}`}
        onClick={(event) => {
            event.stopPropagation();
            onSelect?.();
        }}
        data-testid={id}
    >
        <rect x={x} y={y - MEMORY_HEIGHT / 2} width={MEMORY_WIDTH} height={MEMORY_HEIGHT} rx="10" />
        <text x={x + MEMORY_WIDTH / 2} y={y + 5} textAnchor="middle">DRAM</text>
    </g>
);
