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
import type { TFunction } from 'i18next';
import { METRICS, UNITS, type Cell } from './data';

/** Keep legacy merged metrics and column alignment; textContent safely renders backend names. */
export function matrixTooltip(cell: Cell, t: TFunction): HTMLElement {
    const content = document.createElement('div');
    content.className = 'communication-matrix-tooltip';
    const first = cell.edges[cell.edges.length - 1];
    const appendRow = (label: string, value: string, unit = ''): void => {
        const row = document.createElement('div');
        const caption = document.createElement('span');
        caption.textContent = `${label}:`;
        const detail = document.createElement('span');
        detail.className = 'communication-matrix-tooltip-value';
        detail.textContent = value;
        const suffix = document.createElement('span');
        suffix.className = 'communication-matrix-tooltip-unit';
        suffix.textContent = unit;
        detail.appendChild(suffix);
        row.append(caption, detail);
        content.appendChild(row);
    };
    appendRow('Src Rank -> Dst Rank', `${first.srcRank} -> ${first.dstRank}`);
    if (first.opName) { appendRow(t('operatorName'), first.opName); }
    METRICS.forEach(metric => {
        const values = cell.edges.map(edge => edge[metric]);
        appendRow(t(metric), values.length > 1 ? `[${values.join(', ')}]` : String(values[0]), UNITS[metric]);
    });
    return content;
}
