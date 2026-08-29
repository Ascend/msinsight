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

import type { ThreadMetaData } from '../data';
import type { InsightUnit } from '../insight';
import type { Session } from '../session';
import { MergedThreadData, type ThreadGroup } from '../mergedThreadData';

const DATA_SOURCE: DataSource = {
    remote: 'local',
    port: 0,
    projectName: 'project',
    dataPath: ['trace.db'],
    projectPath: [],
    children: [],
};

const createUnit = (name: string, metadata: Record<string, unknown>): InsightUnit => ({
    name,
    metadata: { dataSource: DATA_SOURCE, ...metadata },
    children: undefined,
    parent: undefined,
    isMerged: false,
    isDisplay: true,
    isUnitVisible: true,
} as unknown as InsightUnit);

const createParent = (cardId = '0'): InsightUnit => createUnit('Process', {
    cardId,
    dbPath: 'trace.db',
    metaType: 'Ascend Hardware',
    processId: 'Ascend Hardware',
    processName: 'Ascend Hardware',
});

const createStream = (threadId: string, cardId = '0'): InsightUnit => createUnit('Thread', {
    cardId,
    dbPath: 'trace.db',
    metaType: 'Ascend Hardware',
    processId: 'Ascend Hardware',
    processName: 'Ascend Hardware',
    threadId,
    threadName: `Stream ${threadId}`,
    groupNameValue: '',
    rankList: [],
});

const attach = (parent: InsightUnit, ...children: InsightUnit[]): void => {
    parent.children = children;
    children.forEach(child => { child.parent = parent; });
};

const createSession = (...parents: InsightUnit[]): Session => ({ units: parents } as unknown as Session);

const autoGroup = (threadIds: string[], cardId = '0'): ThreadGroup => ({
    cardId,
    processId: 'Ascend Hardware',
    threadIds,
});

describe('MergedThreadData automatic merge snapshot', () => {
    it('keeps the automatic snapshot isolated when a manual group overlaps it', () => {
        const data = new MergedThreadData();
        const importedGroup = autoGroup(['2', '1', '2']);
        data.registerAutoThreadGroups([importedGroup]);
        importedGroup.threadIds.push('3');

        data.addMergedGroup(autoGroup(['1', '3']));
        data.registerAutoThreadGroups([autoGroup(['1', '2'])]);

        expect(data.autoMergedThreadGroupList).toEqual([autoGroup(['1', '2'])]);
        expect(data.mergedThreadGroupList).toEqual([autoGroup(['1', '3'])]);

        const source1 = createStream('1');
        const source2 = createStream('2');
        const source3 = createStream('3');
        const synthetic = createStream('');
        (synthetic.metadata as ThreadMetaData).threadIdList = ['1', '2'];
        expect(data.isAutoMergeSource(source1.metadata as ThreadMetaData)).toBe(true);
        expect(data.isAutoMergeSource(source2.metadata as ThreadMetaData)).toBe(true);
        expect(data.isAutoMergeSource(source3.metadata as ThreadMetaData)).toBe(false);
        expect(data.isAutoMergeSource(synthetic.metadata as ThreadMetaData)).toBe(false);
    });

    it('recognizes only a complete automatic merged group regardless of thread order', () => {
        const data = new MergedThreadData();
        data.registerAutoThreadGroups([autoGroup(['1', '2'])]);
        const exact = createStream('');
        const reordered = createStream('');
        const partial = createStream('');
        const manual = createStream('');
        (exact.metadata as ThreadMetaData).threadIdList = ['1', '2'];
        (reordered.metadata as ThreadMetaData).threadIdList = ['2', '1'];
        (partial.metadata as ThreadMetaData).threadIdList = ['1'];
        (manual.metadata as ThreadMetaData).threadIdList = ['1', '3'];

        expect(data.isAutoMergedGroup(exact.metadata as ThreadMetaData)).toBe(true);
        expect(data.isAutoMergedGroup(reordered.metadata as ThreadMetaData)).toBe(true);
        expect(data.isAutoMergedGroup(partial.metadata as ThreadMetaData)).toBe(false);
        expect(data.isAutoMergedGroup(manual.metadata as ThreadMetaData)).toBe(false);
        expect(data.isAutoMergedGroup(createStream('1').metadata as ThreadMetaData)).toBe(false);
    });

    it('returns every complete group regardless of lane visibility', () => {
        const data = new MergedThreadData();
        data.registerAutoThreadGroups([autoGroup(['1', '2']), autoGroup(['3', '4'], '1')]);
        const parent0 = createParent();
        const source1 = createStream('1');
        const source2 = createStream('2');
        source2.isDisplay = false;
        source2.isUnitVisible = false;
        attach(parent0, source1, source2);
        const parent1 = createParent('1');
        const source3 = createStream('3', '1');
        const source4 = createStream('4', '1');
        attach(parent1, source3, source4);

        const session = createSession(parent0, parent1);
        expect(data.getAutoMergeGroupUnits(session)).toEqual([
            { group: autoGroup(['1', '2']), sourceUnits: [source1, source2] },
            { group: autoGroup(['3', '4'], '1'), sourceUnits: [source3, source4] },
        ]);
        expect(data.getAutoMergeGroupUnits(session, [source1])).toEqual([
            { group: autoGroup(['1', '2']), sourceUnits: [source1, source2] },
        ]);
    });

    it('preserves missing member positions when resolving an automatic group', () => {
        const missingData = new MergedThreadData();
        missingData.registerAutoThreadGroups([autoGroup(['1', '2'])]);
        const missingParent = createParent();
        const source1 = createStream('1');
        attach(missingParent, source1);
        expect(missingData.getAutoMergeGroupUnits(createSession(missingParent))).toEqual([
            { group: autoGroup(['1', '2']), sourceUnits: [source1, undefined] },
        ]);
    });

    it('waits for every canonical member before producing an initial automatic merge plan', () => {
        const data = new MergedThreadData();
        data.registerAutoThreadGroups([autoGroup(['1', '2'])]);
        const parent = createParent();
        const source1 = createStream('1');
        attach(parent, source1);
        const session = createSession(parent);
        const assertSpy = jest.spyOn(console, 'assert').mockImplementation(() => {});

        const incompletePlan = data.getNeedMergeThreadLists(session);

        expect(incompletePlan.flat()).toEqual([]);
        expect(source1.isMerged).toBe(false);
        expect(data.mergedThreadGroupList).toEqual([autoGroup(['1', '2'])]);

        const source2 = createStream('2');
        attach(parent, source1, source2);
        expect(data.getNeedMergeThreadLists(session)).toEqual([[source1, source2]]);
        expect(data.mergedThreadGroupList).toEqual([autoGroup(['1', '2'])]);
        assertSpy.mockRestore();
    });

    it('keeps occupied members when resolving an automatic group', () => {
        const conflictData = new MergedThreadData();
        conflictData.registerAutoThreadGroups([autoGroup(['1', '2', '3'])]);
        conflictData.addMergedGroup(autoGroup(['1', '4']));
        const conflictParent = createParent();
        const source1 = createStream('1');
        const source2 = createStream('2');
        const source3 = createStream('3');
        const source4 = createStream('4');
        source1.isMerged = true;
        source4.isMerged = true;
        const manualMerged = createStream('');
        (manualMerged.metadata as ThreadMetaData).threadIdList = ['1', '4'];
        attach(conflictParent, manualMerged, source1, source2, source3, source4);

        expect(conflictData.getAutoMergeGroupUnits(createSession(conflictParent))).toEqual([
            { group: autoGroup(['1', '2', '3']), sourceUnits: [source1, source2, source3] },
        ]);
        expect(conflictData.autoMergedThreadGroupList).toEqual([autoGroup(['1', '2', '3'])]);
        expect(conflictData.mergedThreadGroupList).toEqual([autoGroup(['1', '4'])]);
    });

    it('resolves a complete singleton automatic group', () => {
        const data = new MergedThreadData();
        data.registerAutoThreadGroups([autoGroup(['1'])]);
        const parent = createParent();
        const source = createStream('1');
        attach(parent, source);

        expect(data.getAutoMergeGroupUnits(createSession(parent))).toEqual([
            { group: autoGroup(['1']), sourceUnits: [source] },
        ]);
    });

    it('keeps an active merged group in the current registry during incremental reconciliation', () => {
        const data = new MergedThreadData();
        data.registerAutoThreadGroups([autoGroup(['1', '2'])]);
        const parent = createParent();
        const source1 = createStream('1');
        const source2 = createStream('2');
        source1.isMerged = true;
        source2.isMerged = true;
        const synthetic = createStream('');
        (synthetic.metadata as ThreadMetaData).threadIdList = ['2', '1'];
        attach(parent, synthetic, source1, source2);

        expect(data.getNeedMergeThreadLists(createSession(parent))).toEqual([[]]);
        expect(data.mergedThreadGroupList).toEqual([autoGroup(['1', '2'])]);
    });

    it('prunes removed cards and does not apply their snapshot when a card id is reused', () => {
        const data = new MergedThreadData();
        data.registerAutoThreadGroups([autoGroup(['1', '2']), autoGroup(['3', '4'], '1')]);

        data.removeThreadGroupsByCardIds(['0']);

        expect(data.autoMergedThreadGroupList).toEqual([autoGroup(['3', '4'], '1')]);
        expect(data.mergedThreadGroupList).toEqual([autoGroup(['3', '4'], '1')]);
        expect(data.isAutoMergeSource(createStream('1').metadata as ThreadMetaData)).toBe(false);

        data.registerAutoThreadGroups([autoGroup(['5', '6'])]);
        expect(data.autoMergedThreadGroupList).toEqual([
            autoGroup(['3', '4'], '1'),
            autoGroup(['5', '6']),
        ]);
        expect(data.isAutoMergeSource(createStream('5').metadata as ThreadMetaData)).toBe(true);
    });
});
