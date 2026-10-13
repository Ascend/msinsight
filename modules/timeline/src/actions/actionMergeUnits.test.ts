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

import type { ThreadMetaData } from '../entity/data';
import { chart, type InsightUnit, unit, UnitHeight } from '../entity/insight';
import { MergedThreadData, type ThreadGroup } from '../entity/mergedThreadData';
import type { Session } from '../entity/session';
import {
    actionMergeUnits,
    getAutoMergeActionDisabledReason,
    isAutoMergeActionDisabled,
    mergeUnitsWhenLoadProject,
    tryRestoreAutoMergedGroup,
    unmergeAutoMergedGroup,
} from './actionMergeUnits';

jest.mock('@insight/lib/i18n', () => ({
    __esModule: true,
    default: { t: (key: string): string => key },
}));

const DATA_SOURCE: DataSource = {
    remote: 'local',
    port: 0,
    projectName: 'project',
    dataPath: ['trace.db'],
    projectPath: [],
    children: [],
};

const ThreadTestUnit = unit<ThreadMetaData>({
    name: 'Thread',
    chart: chart({
        type: 'stackStatus',
        height: UnitHeight.STANDARD,
        config: { rowHeight: UnitHeight.STANDARD, isCollapse: true },
        mapFunc: async () => [],
    }),
});

const ProcessTestUnit = unit({ name: 'Process' });

const createThreadUnit = (threadId: string, dbPath = 'trace.db'): InsightUnit => new ThreadTestUnit({
    dataSource: DATA_SOURCE,
    cardId: '0',
    dbPath,
    metaType: 'TEXT',
    processId: 'hardware',
    processName: 'Ascend Hardware',
    threadId,
    threadName: `Stream ${threadId}`,
    groupNameValue: '',
    rankList: [],
});

const createSession = (sourceUnits: InsightUnit[]): Session => {
    const parent = new ProcessTestUnit({ dataSource: DATA_SOURCE });
    parent.phase = 'download';
    parent.children = sourceUnits;
    return {
        phase: 'download',
        units: [parent],
        selectedUnits: [],
        pinnedUnits: [],
        sliceSelection: { active: false, targetUnit: null },
        mergedThreadData: new MergedThreadData(),
        renderTrigger: false,
    } as unknown as Session;
};

const createGroup = (threadIds: string[]): ThreadGroup => ({ cardId: '0', processId: 'hardware', threadIds });

const getMergedUnits = (session: Session): InsightUnit[] =>
    (session.units[0].children ?? []).filter(unit => Array.isArray((unit.metadata as ThreadMetaData).threadIdList));

describe('automatic stream merging', () => {
    it.each([
        ['one member', ['1']],
        ['duplicate members', ['1', '1']],
    ])('keeps the original stream for an automatic group with %s', (_name, threadIds) => {
        const sourceUnit = createThreadUnit('1');
        const session = createSession([sourceUnit]);
        session.mergedThreadData.registerAutoThreadGroups([createGroup(threadIds)]);

        mergeUnitsWhenLoadProject(session);

        expect(session.units[0].children).toEqual([sourceUnit]);
        expect(sourceUnit.isMerged).toBe(false);
        expect(session.mergedThreadData.isAutoMergeSource(sourceUnit.metadata as ThreadMetaData)).toBe(false);
        expect(session.renderTrigger).toBe(false);
    });

    it('ignores a single-member merge saved by an older project', () => {
        const sourceUnit = createThreadUnit('1');
        const session = createSession([sourceUnit]);
        session.mergedThreadData.mergedThreadGroupList = [createGroup(['1'])];

        mergeUnitsWhenLoadProject(session);

        expect(session.units[0].children).toEqual([sourceUnit]);
        expect(sourceUnit.isMerged).toBe(false);
        expect(getMergedUnits(session)).toEqual([]);
    });

    it('initializes, cancels and restores an automatic group with two streams', () => {
        const sourceUnits = [createThreadUnit('1'), createThreadUnit('2')];
        const session = createSession(sourceUnits);
        session.mergedThreadData.registerAutoThreadGroups([createGroup(['1', '2'])]);

        mergeUnitsWhenLoadProject(session);

        const [mergedUnit] = getMergedUnits(session);
        expect(getMergedUnits(session)).toHaveLength(1);
        expect(mergedUnit.isAutoMergedUnit).toBe(true);
        expect((mergedUnit.metadata as ThreadMetaData).threadName).toBe('Stream Merged (1, 2)');
        expect(sourceUnits.every(unit => unit.isMerged)).toBe(true);

        expect(unmergeAutoMergedGroup(session, mergedUnit)).toBe(true);
        expect(getMergedUnits(session)).toEqual([]);
        expect(sourceUnits.every(unit => !unit.isMerged)).toBe(true);

        expect(tryRestoreAutoMergedGroup(session, sourceUnits[0])).toBe(true);
        expect(getMergedUnits(session)).toHaveLength(1);
        expect(getMergedUnits(session)[0].isAutoMergedUnit).toBe(true);
        expect(sourceUnits.every(unit => unit.isMerged)).toBe(true);
    });
});

describe('manual stream merging', () => {
    it.each([false, true])('rejects one selected stream, including a repeated reference: %s', duplicate => {
        const sourceUnit = createThreadUnit('1');
        const session = createSession([sourceUnit]);
        session.selectedUnits = duplicate ? [sourceUnit, sourceUnit] : [sourceUnit];

        actionMergeUnits.perform(session);

        expect(session.units[0].children).toEqual([sourceUnit]);
        expect(sourceUnit.isMerged).toBe(false);
        expect(session.renderTrigger).toBe(false);
    });

    it.each([
        ['different streams in one database', ['1', '2'], ['trace.db', 'trace.db']],
        ['the same thread id in different databases', ['1', '1'], ['first.db', 'second.db']],
    ])('merges %s', (_name, threadIds, dbPaths) => {
        const sourceUnits = threadIds.map((threadId, index) => createThreadUnit(threadId, dbPaths[index]));
        const session = createSession(sourceUnits);
        session.selectedUnits = sourceUnits;

        actionMergeUnits.perform(session);

        const mergedUnits = getMergedUnits(session);
        expect(mergedUnits).toHaveLength(1);
        expect(mergedUnits[0].isAutoMergedUnit).toBe(false);
        expect((mergedUnits[0].metadata as ThreadMetaData).threadSourceList?.map(source => source.dbPath)).toEqual(dbPaths);
        expect(sourceUnits.every(unit => unit.isMerged)).toBe(true);
        expect(session.selectedUnits).toEqual([]);
        expect(session.renderTrigger).toBe(true);
    });

    it('preserves an active manual merge of the same thread id across databases during reconciliation', () => {
        const sourceUnits = [createThreadUnit('1', 'first.db'), createThreadUnit('1', 'second.db')];
        const session = createSession(sourceUnits);
        session.selectedUnits = sourceUnits;
        actionMergeUnits.perform(session);
        const [mergedUnit] = getMergedUnits(session);

        expect(session.mergedThreadData.mergedThreadGroupList).toEqual([createGroup(['1'])]);
        expect(session.mergedThreadData.getNeedMergeThreadLists(session).flat()).toEqual([]);

        expect(session.mergedThreadData.mergedThreadGroupList).toEqual([createGroup(['1'])]);
        expect(session.units[0].children).toEqual([mergedUnit, ...sourceUnits]);
        expect((mergedUnit.metadata as ThreadMetaData).threadSourceList?.map(source => source.dbPath))
            .toEqual(['first.db', 'second.db']);
        expect(sourceUnits.every(unit => unit.isMerged)).toBe(true);
    });
});

describe('automatic merge availability', () => {
    it('blocks cancellation and restoration while any other card is analyzing', () => {
        const sourceUnits = [createThreadUnit('1'), createThreadUnit('2')];
        const session = createSession(sourceUnits);
        session.mergedThreadData.registerAutoThreadGroups([createGroup(['1', '2'])]);
        mergeUnitsWhenLoadProject(session);
        const [mergedUnit] = getMergedUnits(session);
        const parsingCard = new ProcessTestUnit({ dataSource: DATA_SOURCE });
        parsingCard.phase = 'analyzing';
        session.units.push(parsingCard);

        expect(isAutoMergeActionDisabled(session)).toBe(true);
        expect(getAutoMergeActionDisabledReason(session)).toBe('timeline:autoMerge.disabledWhileParsing');
        expect(unmergeAutoMergedGroup(session, mergedUnit)).toBe(false);
        expect(getMergedUnits(session)).toEqual([mergedUnit]);

        parsingCard.phase = 'download';
        expect(unmergeAutoMergedGroup(session, mergedUnit)).toBe(true);
        parsingCard.phase = 'analyzing';

        expect(tryRestoreAutoMergedGroup(session, sourceUnits[0])).toBe(false);
        expect(getMergedUnits(session)).toEqual([]);
        expect(sourceUnits.every(unit => !unit.isMerged)).toBe(true);
    });

    it('reports that the session is not ready before download', () => {
        const session = createSession([createThreadUnit('1')]);
        session.phase = 'configuring';

        expect(isAutoMergeActionDisabled(session)).toBe(true);
        expect(getAutoMergeActionDisabledReason(session)).toBe('timeline:autoMerge.disabledUntilReady');
    });

    it('allows automatic merge actions while only trace or summary data is loading', () => {
        const sourceUnits = [createThreadUnit('1'), createThreadUnit('2')];
        const session = createSession(sourceUnits);
        session.mergedThreadData.registerAutoThreadGroups([createGroup(['1', '2'])]);
        mergeUnitsWhenLoadProject(session);
        const [mergedUnit] = getMergedUnits(session);
        mergedUnit.isTraceLoading = true;
        mergedUnit.isSummaryLoading = true;

        expect(isAutoMergeActionDisabled(session)).toBe(false);
        expect(getAutoMergeActionDisabledReason(session)).toBeUndefined();
        expect(unmergeAutoMergedGroup(session, mergedUnit)).toBe(true);
        sourceUnits.forEach(unit => { unit.isTraceLoading = true; });
        expect(tryRestoreAutoMergedGroup(session, sourceUnits[0])).toBe(true);
        expect(getMergedUnits(session)).toHaveLength(1);
    });
});
