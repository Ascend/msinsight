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

import { renderHook } from '@testing-library/react';
import { message } from 'antd';
import { runInAction } from 'mobx';
import { useSelectUnits } from '../components/ChartContainer/Units/hooks/useSelectUnit';
import type { ThreadMetaData } from '../entity/data';
import { chart, unit, UnitHeight, type InsightUnit } from '../entity/insight';
import { Session } from '../entity/session';
import { getAutoKey } from '../utils/dataAutoKey';
import { actionHideUnits, actionShowHiddenUnits } from './actionHideUnits';
import { actionMergeUnits } from './actionMergeUnits';

jest.mock('../components/ContextMenu', () => {
    const { unit } = jest.requireActual<typeof import('../entity/insight')>('../entity/insight');
    return { EmptyUnit: unit({ name: 'Empty', pinType: 'copied' }) };
});

const dataSource: DataSource = {
    remote: '', port: 0, projectName: '', dataPath: [], projectPath: [], children: [],
};
const ParentUnit = unit({ name: 'Process' });
const ThreadTestUnit = unit<ThreadMetaData>({
    name: 'Thread',
    chart: chart({
        type: 'stackStatus',
        height: UnitHeight.STANDARD,
        mapFunc: async () => [],
        config: { rowHeight: UnitHeight.STANDARD, isCollapse: false },
    }),
});

const createStream = (threadId: string): InsightUnit => new ThreadTestUnit({
    dataSource,
    cardId: '0',
    dbPath: '',
    metaType: 'Ascend Hardware',
    processId: '1',
    processName: 'Ascend Hardware',
    threadId,
    threadName: `Stream ${threadId}`,
    groupNameValue: '',
    rankList: [],
});

const createLanes = (): { session: Session; parent: InsightUnit; streams: InsightUnit[] } => {
    const parent = new ParentUnit({ dataSource });
    const streams = [createStream('1'), createStream('2')];
    parent.children = streams;
    return { session: new Session({ units: [parent] }), parent, streams };
};

const select = (session: Session, selectedUnit: InsightUnit): void => {
    runInAction(() => { session.selectedUnits = [selectedUnit]; });
};

describe('restoring hidden lanes', () => {
    afterEach(() => jest.restoreAllMocks());

    it.each([0, 1])('allows hiding and merging after checking stream %i first', (firstIndex) => {
        const { session, parent, streams } = createLanes();
        const warning = jest.spyOn(message, 'warning');
        select(session, streams[0]);
        actionHideUnits.perform(session);
        const emptyUnit = parent.children?.find(child => child.name === 'Empty') as InsightUnit;
        expect(emptyUnit).toBeDefined();
        expect(streams[0].isUnitVisible).toBe(false);
        select(session, emptyUnit);

        actionShowHiddenUnits.perform(session);

        expect(parent.children).not.toContain(emptyUnit);
        expect(streams[0].isUnitVisible).toBe(true);
        const { result } = renderHook(() => useSelectUnits(session));
        result.current(streams[firstIndex]);
        expect(actionHideUnits.visible(session)).toBe(true);
        expect(session.selectedUnits).toEqual([streams[firstIndex]]);
        expect(session.selectedUnitKeys).toEqual([getAutoKey(streams[firstIndex])]);

        result.current(streams[1 - firstIndex]);
        actionMergeUnits.perform(session);

        expect(warning).not.toHaveBeenCalled();
        expect(streams.every(stream => stream.isMerged)).toBe(true);
        expect(session.mergedThreadData.mergedThreadGroupList).toEqual([
            { cardId: '0', processId: '1', threadIds: expect.arrayContaining(['1', '2']) },
        ]);
    });

    it('clears the removed root placeholder from the selection and selection keys', () => {
        const { session, parent } = createLanes();
        select(session, parent);
        actionHideUnits.perform(session);
        const emptyUnit = session.units.find(child => child.name === 'Empty') as InsightUnit;
        expect(emptyUnit).toBeDefined();
        select(session, emptyUnit);

        actionShowHiddenUnits.perform(session);

        expect(session.units).toEqual([parent]);
        expect(parent.isUnitVisible).toBe(true);
        expect(parent.children?.every(child => child.isUnitVisible)).toBe(true);
        expect(session.selectedUnits).toEqual([]);
        expect(session.selectedUnitKeys).toEqual([]);
    });

    it('keeps the parent selected when restoring its hidden children', () => {
        const { session, parent, streams } = createLanes();
        select(session, streams[0]);
        actionHideUnits.perform(session);
        select(session, parent);

        actionShowHiddenUnits.perform(session);

        expect(parent.children).toEqual(streams);
        expect(streams[0].isUnitVisible).toBe(true);
        expect(session.selectedUnits).toEqual([parent]);
        expect(session.selectedUnitKeys).toEqual([getAutoKey(parent)]);
    });
});
