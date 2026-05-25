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

import { actionPinByUnitName, actionUnpinByUnitName } from './actionPinUnits';
import { switchPinned } from '../components/ChartContainer/unitPin';
import { unit } from '../entity/insight';
import type { EmptyMetaData, ThreadMetaData } from '../entity/data';
import type { InsightUnit } from '../entity/insight';

const mockDataSource = {
    remote: '',
    port: 0,
    projectName: '',
    dataPath: [],
    projectPath: [],
    children: [],
};

const ThreadTestUnit = unit<ThreadMetaData>({
    name: 'Thread',
});

const EmptyTestUnit = unit<EmptyMetaData>({
    name: 'Empty',
});

function createThreadUnit({
    threadId = '1',
    threadName = 'stream1',
    groupNameValue = '',
    threadIdList,
}: Partial<ThreadMetaData> = {}): InsightUnit {
    const metadata: ThreadMetaData = {
        dataSource: mockDataSource,
        cardId: '0',
        dbPath: '',
        metaType: 'TEXT',
        threadId,
        threadName,
        groupNameValue,
        threadIdList,
        rankList: [],
    };

    return new ThreadTestUnit(metadata);
}

function createEmptyUnit(count: number = 1): InsightUnit {
    return new EmptyTestUnit({
        count,
        dataSource: mockDataSource,
    });
}

function createSession(selectedUnits: InsightUnit[], pinnedUnits: InsightUnit[] = []): any {
    return {
        selectedUnits,
        pinnedUnits,
    };
}

describe('actionPinUnits visible', () => {
    // 普通未置顶的叶子泳道应显示一键置顶菜单
    it('shows pin by unit name for normal unpinned leaf units', () => {
        const unit = createThreadUnit();
        const session = createSession([unit]);

        expect(actionPinByUnitName.visible?.(session)).toBe(true);
    });

    // 隐藏汇总泳道上不应显示一键置顶菜单
    it('hides pin by unit name on hidden summary unit', () => {
        const unit = createEmptyUnit();
        const session = createSession([unit]);

        expect(actionPinByUnitName.visible?.(session)).toBe(false);
    });

    // 合并泳道上不应显示一键置顶菜单
    it('hides pin by unit name on merged unit', () => {
        const unit = createThreadUnit({
            threadId: '',
            threadName: 'Stream Merged (1, 2)',
            threadIdList: ['1', '2'],
        });
        const session = createSession([unit]);

        expect(actionPinByUnitName.visible?.(session)).toBe(false);
    });

    // 普通已置顶的叶子泳道应显示取消一键置顶菜单
    it('shows unpin by unit name for pinned normal leaf units', () => {
        const unit = createThreadUnit();
        switchPinned(unit);
        const session = createSession([unit], [unit]);

        expect(actionUnpinByUnitName.visible?.(session)).toBe(true);
    });

    // 已置顶的隐藏汇总泳道上不应显示取消一键置顶菜单
    it('hides unpin by unit name on hidden summary unit even if pinned', () => {
        const unit = createEmptyUnit();
        switchPinned(unit);
        const session = createSession([unit], [unit]);

        expect(actionUnpinByUnitName.visible?.(session)).toBe(false);
    });

    // 已置顶的合并泳道上不应显示取消一键置顶菜单
    it('hides unpin by unit name on merged unit even if pinned', () => {
        const unit = createThreadUnit({
            threadId: '',
            threadName: 'Stream Merged (1, 2)',
            threadIdList: ['1', '2'],
        });
        switchPinned(unit);
        const session = createSession([unit], [unit]);

        expect(actionUnpinByUnitName.visible?.(session)).toBe(false);
    });
});
