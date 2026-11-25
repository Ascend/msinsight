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
import { updateSessionLineData } from './FilterLinkLine';
import type { Session } from '../entity/session';

jest.mock('../insight/units/AscendUnit', () => ({}));

it.each(['change', 'exit', 'generation', 'in-place'] as const)('discards late flow results after %s', async change => {
    let complete: (value: []) => void = () => {};
    const pending = new Promise<[]>(resolve => { complete = resolve; });
    const fetch = jest.fn().mockReturnValueOnce(pending).mockResolvedValue([]);
    const session = {
        isTimeAnalysisMode: true,
        timeAnalysisRange: [10, 20],
        ridLineType: '',
        linkLines: { current: [] },
        singleLinkLine: { current: [] },
        renderTrigger: false,
    } as unknown as Session;
    const generation = { current: 0 };
    const update = updateSessionLineData(['flow'], new Map([['flow', fetch]]), session, generation);
    const oldRequest = update();
    if (change === 'exit') { session.isTimeAnalysisMode = false; }
    if (change === 'change') { session.timeAnalysisRange = [30, 40]; }
    if (change === 'generation') { generation.current++; }
    if (change === 'in-place' && session.timeAnalysisRange !== undefined) { session.timeAnalysisRange[0] = 30; }

    // The current request finishes first. An older result must not replace it.
    await update();
    expect(session.linkLines.flow).toEqual([]);
    const currentLines = session.linkLines;
    const currentRender = session.renderTrigger;
    complete([]);
    await oldRequest;

    expect(session.linkLines).toBe(currentLines);
    expect(session.renderTrigger).toBe(currentRender);
});
