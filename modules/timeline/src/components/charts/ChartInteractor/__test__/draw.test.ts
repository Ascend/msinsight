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

import type { Theme } from '@emotion/react';
import type { Session } from '../../../../entity/session';
import { unitBase, UnitHeight, type InsightUnit } from '../../../../entity/insight';
import { getLaneProcessIdentity } from '../../../../entity/data';
import type { DataBlock, FlowEvent } from '../../../FilterLinkLine';
import { draw, getHeight, processIsCol, UNDRAW_HEIGHT, type DrawCanvasArgs } from '../draw';

// Use D3's bundled build because pnpm's nested ESM packages are not transformed by Jest.
jest.mock('d3', () => jest.requireActual(require.resolve('d3').replace(/src[\\/]index\.js$/, 'dist/d3.min.js')));
jest.mock('@insight/lib/theme', () => ({ ThemeName: { DARK: 'dark', LIGHT: 'light' } }), { virtual: true });
jest.mock('../../../ChartContainer/ChartContainer', () => ({ TIME_LINE_AXIS_HEIGHT_PX: 30 }));
jest.mock('../../../ChartContainer/Units/Units', () => ({ getClassNameByMetadata: jest.fn() }));
jest.mock('../../TimelineAxis', () => ({ getTextParser: jest.fn() }));

const TestUnit = unitBase({ name: 'Test Unit', collapsible: true });

const createUnit = (metadata: Record<string, unknown>, children: InsightUnit[] = [], height: number = UnitHeight.UPPER): InsightUnit => {
    const unit = new TestUnit({
        cardId: 'rank0',
        dataSource: { remote: '', port: 0, projectName: '', dataPath: [], projectPath: [], children: [] },
        ...metadata,
    });
    unit.children = children;
    unit.isExpanded = true;
    unit.height = () => height;
    return unit;
};

const createScene = (dbPath?: string, pythonPosition: 'above' | 'below' = 'above'): {
    card: InsightUnit;
    thread: InsightUnit;
    cann: InsightUnit;
    python: InsightUnit;
    flow: FlowEvent;
    pythonPoint: DataBlock;
} => {
    const host = { processId: '4294967297', dbPath };
    const acl = createUnit({ ...host, threadId: 'acl' }, [], UnitHeight.STANDARD * 4);
    const cann = createUnit({ ...host, processName: 'CANN' }, [acl]);
    const mstx = createUnit({ ...host, processName: 'MSTX', threadId: '1' }, [
        createUnit({ ...host, threadId: 'domain0' }),
    ]);
    // DB metadata places CANN API lanes directly under Thread when Python Stack is present.
    const thread = createUnit({ ...host, processName: 'Thread 1', threadId: '1' }, [acl, mstx]);
    thread.isExpanded = false;
    const python = createUnit({ ...host, processName: 'Thread 1', threadId: 'python_stack:4294967297' }, [], UnitHeight.STANDARD * 4);
    const process = createUnit({ ...host, processName: 'Process 1' }, pythonPosition === 'above' ? [python, thread] : [thread, python]);
    const stream = createUnit({ processId: 'device', threadId: 'stream0', dbPath }, [], UnitHeight.STANDARD * 4);
    const device = createUnit({ processId: 'device', dbPath }, [stream]);
    return {
        card: createUnit({ dbPath }, [process, device]),
        thread,
        cann,
        python,
        flow: {
            category: 'HostToDevice',
            cardId: 'rank0',
            from: { pid: host.processId, tid: 'acl', timestamp: 10, depth: 2, dbPath },
            to: { pid: 'device', tid: 'stream0', timestamp: 50, depth: 1, dbPath },
        },
        pythonPoint: { pid: host.processId, tid: 'python_stack:4294967297', timestamp: 20, depth: 2, dbPath },
    };
};

const createSession = (units: InsightUnit[], flows: FlowEvent[], mode: 'single' | 'all' = 'single'): Session => ({
    units,
    scrollTop: 0,
    sliceSelection: { active: false },
    drawLineMode: mode,
    singleLinkLine: { HostToDevice: flows },
    linkLines: { HostToDevice: flows },
    linkLineCategories: ['HostToDevice'],
    ridLineType: '',
    domainRange: { domainStart: 0, domainEnd: 100 },
    unitsConfig: { offsetConfig: { timestampOffset: {} } },
} as unknown as Session);

const drawScene = (session: Session): jest.Mock => {
    const bezierCurveTo = jest.fn();
    const ctx = {
        canvas: { width: 1000, height: 2000, clientWidth: 1000, clientHeight: 2000 },
        clearRect: jest.fn(),
        save: jest.fn(),
        restore: jest.fn(),
        beginPath: jest.fn(),
        closePath: jest.fn(),
        rect: jest.fn(),
        clip: jest.fn(),
        moveTo: jest.fn(),
        lineTo: jest.fn(),
        stroke: jest.fn(),
        fill: jest.fn(),
        bezierCurveTo,
    } as unknown as CanvasRenderingContext2D;
    draw({ ctx, width: 1000, height: 2000, session, theme: { colorPalette: {} } as Theme } as DrawCanvasArgs);
    return bezierCurveTo;
};

describe.each(['above', 'below'] as const)('link endpoints with Python Stack %s its sibling Thread', pythonPosition => {
    const createOrderedScene = (dbPath?: string): ReturnType<typeof createScene> => createScene(dbPath, pythonPosition);
    const pythonTop = (pythonPosition === 'above' ? 2 : 3) * (UnitHeight.UPPER + 1);
    const threadTop = 2 * (UnitHeight.UPPER + 1) + (pythonPosition === 'above' ? UnitHeight.STANDARD * 4 + 1 : 0);

    it.each([undefined, 'rank0.db'])('anchors hidden CANN to the collapsed Thread (dbPath: %s)', dbPath => {
        const { card, flow, pythonPoint } = createOrderedScene(dbPath);
        const session = createSession([card], [flow, { ...flow, from: pythonPoint }]);
        const drawCurve = drawScene(session);
        const streamTop = 4 * (UnitHeight.UPPER + 1) + UnitHeight.STANDARD * 4 + 1;

        expect(drawCurve).toHaveBeenCalledTimes(2);
        expect(drawCurve.mock.calls[0][1]).toBe(UNDRAW_HEIGHT + threadTop + UnitHeight.UPPER / 2);
        expect(drawCurve.mock.calls[0][5]).toBe(UNDRAW_HEIGHT + streamTop + 1.5 * UnitHeight.STANDARD);
        expect(drawCurve.mock.calls[1][1]).toBe(UNDRAW_HEIGHT + pythonTop + 2.5 * UnitHeight.STANDARD);
    });

    it('uses the collapsed Thread anchor when all category links are drawn', () => {
        const { card, flow } = createOrderedScene();
        const drawCurve = drawScene(createSession([card], [flow], 'all'));

        expect(drawCurve).toHaveBeenCalledTimes(1);
        expect(drawCurve.mock.calls[0][1]).toBe(UNDRAW_HEIGHT + threadTop + UnitHeight.UPPER / 2);
    });

    it('updates the endpoint when the Thread is expanded and collapsed again', () => {
        const { card, thread, flow } = createOrderedScene();
        const session = createSession([card], [flow]);
        drawScene(session);
        const collapsedHeight = getHeight(session, flow.from, 'rank0', flow.category);

        thread.isExpanded = true;
        drawScene(session);
        expect(processIsCol.get(getLaneProcessIdentity('rank0', flow.from.pid))).toBeUndefined();
        expect(getHeight(session, flow.from, 'rank0', flow.category)).toBe(
            UNDRAW_HEIGHT + threadTop + UnitHeight.UPPER + 1 + 2.5 * UnitHeight.STANDARD,
        );

        thread.isExpanded = false;
        drawScene(session);
        expect(getHeight(session, flow.from, 'rank0', flow.category)).toBe(collapsedHeight);
    });

    it('keeps the CANN fallback when only CANN is collapsed and MSTX remains visible', () => {
        const { card, thread, cann, flow, pythonPoint } = createOrderedScene();
        thread.children = [cann, ...(thread.children?.slice(1) ?? [])];
        thread.isExpanded = true;
        cann.isExpanded = false;
        const session = createSession([card], [flow]);
        drawScene(session);

        expect(getHeight(session, flow.from, 'rank0', flow.category)).toBe(
            UNDRAW_HEIGHT + threadTop + UnitHeight.UPPER + 1 + UnitHeight.UPPER / 2,
        );
        const expandedPythonTop = (pythonPosition === 'above' ? 2 : 6) * (UnitHeight.UPPER + 1);
        expect(getHeight(session, pythonPoint, 'rank0', flow.category)).toBe(
            UNDRAW_HEIGHT + expandedPythonTop + 2.5 * UnitHeight.STANDARD,
        );
    });

    it('keeps distinct endpoints when both Python Stack and Thread are collapsed', () => {
        const { card, python, flow, pythonPoint } = createOrderedScene();
        python.isExpanded = false;
        python.height = () => UnitHeight.COLL;
        const session = createSession([card], [flow, { ...flow, from: pythonPoint }]);
        const drawCurve = drawScene(session);
        const collapsedThreadTop = 2 * (UnitHeight.UPPER + 1) + (pythonPosition === 'above' ? UnitHeight.COLL + 1 : 0);

        expect(drawCurve).toHaveBeenCalledTimes(2);
        expect(drawCurve.mock.calls[0][1]).toBe(UNDRAW_HEIGHT + collapsedThreadTop + UnitHeight.UPPER / 2);
        expect(drawCurve.mock.calls[1][1]).toBe(UNDRAW_HEIGHT + pythonTop + UnitHeight.COLL / 2);
    });

    it('does not share collapsed anchors across source databases with the same process id', () => {
        const first = createOrderedScene('first.db');
        const second = createOrderedScene('second.db');
        second.thread.isExpanded = true;
        const session = createSession([first.card, second.card], [first.flow, second.flow]);
        const drawCurve = drawScene(session);
        const firstCardHeight = 4 * (UnitHeight.UPPER + 1) + 2 * (UnitHeight.STANDARD * 4 + 1);

        expect(drawCurve).toHaveBeenCalledTimes(2);
        expect(getHeight(session, first.flow.from, 'rank0', first.flow.category)).toBe(
            UNDRAW_HEIGHT + threadTop + UnitHeight.UPPER / 2,
        );
        expect(getHeight(session, second.flow.from, 'rank0', second.flow.category)).toBe(
            UNDRAW_HEIGHT + firstCardHeight + threadTop + UnitHeight.UPPER + 1 + 2.5 * UnitHeight.STANDARD,
        );
    });
});

it('anchors a non-async flow to each nearest collapsed ancestor in the same process', () => {
    const metadata = { processId: 'host-process' };
    const sourceLeaf = createUnit({ ...metadata, threadId: 'source-thread' });
    const targetLeaf = createUnit({ ...metadata, threadId: 'target-thread' });
    const sourceGroup = createUnit(metadata, [sourceLeaf]);
    const targetGroup = createUnit(metadata, [targetLeaf]);
    sourceGroup.isExpanded = false;
    targetGroup.isExpanded = false;
    const process = createUnit(metadata, [sourceGroup, targetGroup]);
    const card = createUnit({}, [process]);
    const flow: FlowEvent = {
        category: 'HostToDevice',
        cardId: 'rank0',
        from: { pid: metadata.processId, tid: 'source-thread', timestamp: 10, depth: 3 },
        to: { pid: metadata.processId, tid: 'target-thread', timestamp: 20, depth: 4 },
    };

    const drawCurve = drawScene(createSession([card], [flow], 'all'));
    const sourceGroupTop = 2 * (UnitHeight.UPPER + 1);
    const targetGroupTop = sourceGroupTop + UnitHeight.UPPER + 1;

    expect(drawCurve).toHaveBeenCalledTimes(1);
    expect(drawCurve.mock.calls[0][1]).toBe(UNDRAW_HEIGHT + sourceGroupTop + UnitHeight.UPPER / 2);
    expect(drawCurve.mock.calls[0][5]).toBe(UNDRAW_HEIGHT + targetGroupTop + UnitHeight.UPPER / 2);
});

it('keeps async_task_queue on its process parent until the matching PyTorch lane is available', () => {
    const source = createUnit({ processId: '43476' });
    source.isExpanded = false;
    const targetPytorch = createUnit({ processId: '43590', threadId: 'pytorch' }, [], UnitHeight.STANDARD);
    const target = createUnit({ processId: '43590' }, [targetPytorch]);
    const card = createUnit({}, [source, target]);
    const flow: FlowEvent = {
        category: 'async_task_queue',
        cardId: 'rank0',
        from: { pid: '43476', tid: 'pytorch', timestamp: 10, depth: 0 },
        to: { pid: '43590', tid: 'pytorch', timestamp: 20, depth: 0 },
    };

    const session = createSession([card], [flow], 'all');
    session.linkLines = { async_task_queue: [flow as unknown as Record<string, unknown>] };
    session.linkLineCategories = ['async_task_queue'];
    const drawCurve = drawScene(session);
    const sourceTop = UnitHeight.UPPER + 1;
    const targetPytorchTop = 3 * (UnitHeight.UPPER + 1);

    expect(drawCurve).toHaveBeenCalledTimes(1);
    expect(drawCurve.mock.calls[0][1]).toBe(UNDRAW_HEIGHT + sourceTop + UnitHeight.UPPER / 2);
    expect(drawCurve.mock.calls[0][5]).toBe(UNDRAW_HEIGHT + targetPytorchTop + UnitHeight.STANDARD / 2);
});
