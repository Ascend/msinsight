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
import type { TFunction } from 'i18next';

interface DiagramLegendProps {
    visible: boolean;
    t: TFunction<'numa'>;
}

export const DiagramLegend: React.FC<DiagramLegendProps> = ({ visible, t }) => {
    if (!visible) return null;
    return (
        <div className="legend">
            <span><i className="legend-line" />{t('communicationLink')}</span>
            <span><i className="legend-node numa" />NUMA</span>
            <span><i className="legend-node dram" />DRAM</span>
        </div>
    );
};
