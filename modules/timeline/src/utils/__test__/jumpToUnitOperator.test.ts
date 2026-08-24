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

import type { OpDetail } from '../../api/interface';
import { calculateDomainRange } from '../../components/CategorySearch';
import type { ThreadMetaData } from '../../entity/data';
import type { InsightUnit } from '../../entity/insight';
import type { Session } from '../../entity/session';
import { ThreadUnit } from '../../insight/units/AscendUnit';
import { getTimeOffset } from '../../insight/units/utils';
import { store } from '../../store';
import jumpToUnitOperator from '../jumpToUnitOperator';

jest.mock('../../components/CategorySearch', () => ({
    calculateDomainRange: jest.fn(() => [80, 140]),
}));

jest.mock('../../insight/units/utils', () => ({
    colorPalette: ['deepBlue'],
    getTimeOffset: jest.fn(() => 5),
}));

jest.mock('../../insight/units/AscendUnit', () => ({
    ThreadUnit: class MockThreadUnit {
        metadata: unknown;

        constructor(metadata: unknown) {
            this.metadata = metadata;
        }
    },
}));

jest.mock('../../store', () => ({
    store: {
        sessionStore: {
            activeSession: undefined,
        },
    },
}));

const createSession = (): Session => ({
    startRecordTime: 10,
    domainRange: { domainStart: 0, domainEnd: 100 },
    locateUnit: undefined,
    selectedData: undefined,
    foregroundTarget: undefined,
} as unknown as Session);

const createMergedUnit = (): InsightUnit => new ThreadUnit({
    cardId: '0',
    dbPath: 'trace.db',
    processId: '3513236896',
    metaType: 'Ascend Hardware',
    threadId: '',
    threadIdList: ['2', '8'],
    threadName: 'Stream Merged (2, 8)',
} as ThreadMetaData) as InsightUnit;

const OP_DETAIL: OpDetail = {
    id: '42',
    cardId: '0',
    dbPath: 'trace.db',
    tid: '2',
    pid: '3513236896',
    depth: 0,
    duration: 20,
    name: 'aclnnInplaceMuls_MulAiCore_Mul',
    timestamp: 100,
    metaType: 'Ascend Hardware',
};

describe('jumpToUnitOperator', () => {
    beforeEach(() => {
        store.sessionStore.activeSession = createSession();
        (getTimeOffset as jest.Mock).mockReturnValue(5);
        (calculateDomainRange as jest.Mock).mockReturnValue([80, 140]);
    });

    it('preserves the source slice and foreground target when jumping into a merged unit', () => {
        const mergedUnit = createMergedUnit();
        const siblingMergedUnit = new ThreadUnit({
            ...(mergedUnit.metadata as ThreadMetaData),
            threadIdList: ['3', '8'],
        } as ThreadMetaData) as InsightUnit;

        jumpToUnitOperator(OP_DETAIL);

        const locateUnit = store.sessionStore.activeSession?.locateUnit;
        expect(locateUnit?.target(mergedUnit)).toBe(true);
        expect(locateUnit?.target(siblingMergedUnit)).toBe(false);
        locateUnit?.onSuccess(mergedUnit);

        expect(store.sessionStore.activeSession?.selectedData).toEqual(expect.objectContaining({
            id: '42',
            threadId: '2',
            processId: '3513236896',
            cardId: '0',
            startTime: 95,
        }));
        expect(store.sessionStore.activeSession?.selectedData?.threadId).not.toBe('');
        expect(store.sessionStore.activeSession?.selectedDataUnit).toBe(mergedUnit);
        expect(store.sessionStore.activeSession?.foregroundTarget).toEqual({
            rankId: '0',
            dbPath: 'trace.db',
            pid: '3513236896',
            tid: '2',
            id: '42',
            name: 'aclnnInplaceMuls_MulAiCore_Mul',
            startTime: 100,
            duration: 20,
            depth: 0,
            metaType: 'Ascend Hardware',
        });
        expect(store.sessionStore.activeSession?.domainRange).toEqual({ domainStart: 80, domainEnd: 140 });
    });
});
