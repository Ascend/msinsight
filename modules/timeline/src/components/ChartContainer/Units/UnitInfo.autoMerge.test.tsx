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
import { act, fireEvent, render } from '@testing-library/react';
import { message } from 'antd';
import { runInAction } from 'mobx';
import i18n from '@insight/lib/i18n';
import { Session } from '../../../entity/session';
import { chart, unit, UnitHeight, type InsightUnit } from '../../../entity/insight';
import type { ThreadMetaData } from '../../../entity/data';
import { mergeUnitsWhenLoadProject } from '../../../actions/actionMergeUnits';
import { UnitInfo } from './UnitInfo';

jest.mock('../../../insight/units/AscendUnit', () => ({
    CardUnit: jest.fn(),
    ThreadUnit: jest.fn(),
    ROOT_UNIT: jest.fn(),
}));

jest.mock('@insight/lib/components', () => {
    const { Checkbox, Tooltip } = jest.requireActual('antd');
    return { Checkbox, Tooltip };
});

jest.mock('../../../connection', () => ({
    __esModule: true,
    default: { addListener: jest.fn() },
}));

const dataSource: DataSource = {
    remote: 'local', port: 0, projectName: 'test', dataPath: ['trace.db'], projectPath: [], children: [],
};

// Keep the real observable lane model and merge actions, without mounting trace charts.
const TestThread = unit<ThreadMetaData>({
    name: 'Thread',
    renderInfo: (_session, metadata) => metadata.threadName,
    chart: chart({
        type: 'stackStatus',
        height: UnitHeight.STANDARD,
        config: { rowHeight: UnitHeight.STANDARD, isCollapse: false },
        mapFunc: async () => [],
    }),
});
const TestParent = unit({ name: 'Process' });
const TestCard = unit({ name: 'Card' });

const createScenario = (merged: boolean): { session: Session; parent: InsightUnit; sources: InsightUnit[]; otherCard: InsightUnit } => {
    const session = new Session();
    session.phase = 'download';
    const parent = new TestParent({ dataSource });
    const sources = ['1', '2'].map(threadId => new TestThread({
        dataSource,
        cardId: '0',
        dbPath: 'trace.db',
        metaType: 'Ascend Hardware',
        processId: 'Ascend Hardware',
        processName: 'Ascend Hardware',
        threadId,
        threadName: `Stream ${threadId}`,
        groupNameValue: '',
        rankList: [],
    }));
    parent.children = sources;
    const readyCard = new TestCard({ dataSource, cardId: '0' });
    readyCard.phase = 'download';
    readyCard.children = [parent];
    const otherCard = new TestCard({ dataSource, cardId: '1' });
    otherCard.phase = 'download';
    session.units = [readyCard, otherCard];
    session.mergedThreadData.registerAutoThreadGroups([
        { cardId: '0', processId: 'Ascend Hardware', threadIds: ['1', '2'] },
    ]);
    if (merged) {
        mergeUnitsWhenLoadProject(session);
    }
    return { session, parent, sources, otherCard };
};

const renderUnit = (session: Session, lane: InsightUnit): ReturnType<typeof render> => render(
    <UnitInfo session={session} unit={lane} laneInfoWidth={300} hasExpandIcon={false} hasPinButton={false}
        isPinned={false} height={24} className="" isSelected={false} onMouseDown={jest.fn()} />,
);

describe('automatic merge buttons while another card is parsing', () => {
    const warning = jest.spyOn(message, 'warning').mockImplementation(jest.fn());

    it.each([
        { merged: false, buttonId: 'auto-merge-source-button' },
        { merged: true, buttonId: 'auto-unmerge-group-button' },
    ])('keeps $buttonId visible and explains why it is disabled', async ({ merged, buttonId }) => {
        const { session, parent, sources, otherCard } = createScenario(merged);
        otherCard.phase = 'analyzing';
        const lane = parent.children?.[0] as InsightUnit;
        const childrenBefore = [...parent.children ?? []];
        const groupsBefore = session.mergedThreadData.mergedThreadGroupList.map(group => ({ ...group, threadIds: [...group.threadIds] }));
        const view = renderUnit(session, lane);

        const button = view.getByTestId(buttonId);
        expect(button).toHaveAttribute('aria-disabled', 'true');
        expect(button).toBeVisible();
        fireEvent.mouseEnter(button);
        const reason = i18n.t('timeline:autoMerge.disabledWhileParsing');
        expect(await view.findByRole('tooltip')).toHaveTextContent(reason);
        fireEvent.mouseDown(button);
        fireEvent.mouseUp(button);
        fireEvent.click(button);

        expect(warning).toHaveBeenCalledWith(reason);
        expect(parent.children).toEqual(childrenBefore);
        expect(sources.every(source => source.isMerged === merged)).toBe(true);
        expect(session.mergedThreadData.mergedThreadGroupList).toEqual(groupsBefore);
        expect(session.selectedUnits).toEqual([]);

        act(() => { runInAction(() => { otherCard.phase = 'download'; }); });
        expect(button).toHaveAttribute('aria-disabled', 'false');
        fireEvent.click(button);

        expect(warning).toHaveBeenCalledTimes(1);
        expect(sources.every(source => source.isMerged !== merged)).toBe(true);
        if (merged) {
            expect(parent.children).toEqual(sources);
        } else {
            expect(parent.children).toHaveLength(3);
            expect(parent.children?.[0].metadata.threadIdList).toEqual(['1', '2']);
        }
    });

    it('allows restoring a group during an ordinary trace refresh', () => {
        const { session, parent, sources } = createScenario(false);
        sources[0].isTraceLoading = true;
        sources[0].isSummaryLoading = true;
        const view = renderUnit(session, sources[0]);

        const button = view.getByTestId('auto-merge-source-button');
        expect(button).toHaveAttribute('aria-disabled', 'false');
        fireEvent.click(button);

        expect(warning).not.toHaveBeenCalled();
        expect(parent.children).toHaveLength(3);
        expect(parent.children?.[0].isAutoMergedUnit).toBe(true);
        expect(sources.every(source => source.isMerged)).toBe(true);
    });
});
