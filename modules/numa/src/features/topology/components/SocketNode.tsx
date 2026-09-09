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
import type { SocketLayout } from '../model/types';

interface Props {
    layout: SocketLayout;
    selected: boolean;
    onSelect: () => void;
}

export const SocketNode: React.FC<Props> = ({ layout, selected, onSelect }) => (
    <g
        className={`socket ${selected ? 'selected' : ''}`}
        onClick={(event) => {
            event.stopPropagation();
            onSelect();
        }}
        data-testid={`socket-${layout.socket.id}`}
    >
        <rect x={layout.x} y={layout.y} width={layout.width} height={layout.height} rx="20" />
    </g>
);
