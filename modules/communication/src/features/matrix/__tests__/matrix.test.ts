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
import { createInstance } from 'i18next';
import zh from '../../../../../lib/src/i18n/communication/zh.json';
import en from '../../../../../lib/src/i18n/communication/en.json';
import { matrixCells } from '../data';
import { matrixTooltip } from '../tooltip';
import { updateMatrixLabels } from '../options';

const edges = [
    { srcRank: 1, dstRank: 2, bandwidth: 1.123456, transitSize: 3, transitTime: 5, transportType: 'RDMA', opName: '<img src=x onerror=alert(1)>' },
    { srcRank: 1, dstRank: 2, bandwidth: 9, transitSize: 6, transitTime: 8, transportType: 'PCIE', opName: 'second record' },
];
const i18n = createInstance();
beforeAll(() => {
    void i18n.init({ lng: 'zh', initImmediate: false, resources: { zh, en } });
});
it('merges duplicate link metrics in the legacy order and safely displays the first operator name', () => {
    const cell = matrixCells(edges, [1, 2], 'bandwidth', false)[0];
    const tooltip = matrixTooltip(cell, i18n.getFixedT('zh', 'communication'));
    expect([...tooltip.children].map(row => row.textContent)).toEqual([
        'Src Rank -> Dst Rank:1 -> 2', '算子名称:<img src=x onerror=alert(1)>',
        '带宽:[9, 1.123456](GB/s)', '传输大小:[6, 3](MB)', '链路方式:[PCIE, RDMA]', '传输时长:[8, 5](ms)',
    ]);
    expect(tooltip.querySelector('img')).toBeNull();
});
it('shows only range-matched values and uses translated labels', () => {
    const cell = matrixCells(edges, [1, 2], 'bandwidth', false, [9, 9])[0];
    const tooltip = matrixTooltip(cell, i18n.getFixedT('en', 'communication'));
    expect(tooltip.textContent).toContain('OperatorName:second record');
    expect(tooltip.textContent).toContain('Bandwidth:9(GB/s)');
    expect(tooltip.textContent).not.toContain('1.123456');
});
it('omits an empty operator name while retaining all metrics', () => {
    const cell = matrixCells([{ ...edges[0], opName: '' }], [1, 2], 'transportType', false)[0];
    const tooltip = matrixTooltip(cell, i18n.getFixedT('zh', 'communication'));
    expect(tooltip.children).toHaveLength(5);
    expect(tooltip.textContent).toContain('链路方式:RDMA');
});
it('bases cell labels on visible source ranks, including when only the Y axis changes', () => {
    const setOption = jest.fn();
    const chart = { getOption: () => ({ xAxis: [{ data: Array.from({ length: 32 }, (_, i) => i) }], dataZoom: [{ start: 0, end: 100 }, { start: 0, end: 10 }] }), setOption };
    updateMatrixLabels(chart);
    expect(setOption).toHaveBeenLastCalledWith({ series: [{ label: { show: false } }] });
    chart.getOption = () => ({ xAxis: [{ data: Array.from({ length: 32 }, (_, i) => i) }], dataZoom: [{ start: 0, end: 50 }, { start: 0, end: 10 }] });
    updateMatrixLabels(chart);
    expect(setOption).toHaveBeenLastCalledWith({ series: [{ label: { show: true } }] });
});
