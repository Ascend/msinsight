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
import i18n from '@insight/lib/i18n';
import { getDefaultChartOptions } from '@insight/lib/utils';
import type { CommunicationWebGLOperator } from './dataIndex';
import { formatTime } from './options';

/** Preserve the existing thumbnail tooltip layout; DOM text keeps operator names safe. */
export class OperatorTooltip {
    private readonly element: HTMLDivElement;
    constructor(private readonly container: HTMLElement) {
        this.element = document.createElement('div');
        this.element.className = 'operator-timeline-tooltip';
        this.element.setAttribute('role', 'tooltip');
        this.element.style.fontFamily = getDefaultChartOptions().textStyle.fontFamily;
        container.appendChild(this.element);
    }

    show(operator: CommunicationWebGLOperator, rankId: string, color: string, x: number, y: number): void {
        const dot = document.createElement('span');
        dot.className = 'operator-timeline-tooltip-dot';
        dot.style.backgroundColor = color;
        this.element.replaceChildren(dot, document.createTextNode(' '));
        const fields = [
            ['Rank ID', rankId], ['Operator Name', operator.operatorName],
            ['Start Time', `${formatTime(operator.startTime)}ms`], ['Elapse Time', `${formatTime(operator.duration)}ms`],
        ];
        fields.forEach(([label, value]) => {
            const strong = document.createElement('strong');
            strong.textContent = value;
            this.element.append(document.createTextNode(`${i18n.t(`tableHead.${label}`, { ns: 'communication' })}: `), strong, document.createElement('br'));
        });
        this.element.style.borderColor = color;
        this.element.style.display = 'block';
        this.element.style.left = `${Math.min(x + 12, Math.max(0, this.container.clientWidth - this.element.offsetWidth - 8))}px`;
        this.element.style.top = `${Math.min(y + 12, Math.max(0, this.container.clientHeight - this.element.offsetHeight - 8))}px`;
    }

    hide(): void { this.element.style.display = 'none'; }
    dispose(): void { this.element.remove(); }
}
