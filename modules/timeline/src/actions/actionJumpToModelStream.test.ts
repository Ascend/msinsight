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

import { message } from 'antd';
import type { ThreadMetaData } from '../entity/data';
import type { InsightUnit } from '../entity/insight';
import type { SelectedDataType, Session } from '../entity/session';
import { ThreadUnit } from '../insight/units/AscendUnit';
import { getTimeOffset } from '../insight/units/utils';
import { actionJumpToModelStream } from './actionJumpToModelStream';

jest.mock('antd', () => ({
    message: {
        warning: jest.fn(),
    },
}));

jest.mock('@insight/lib/i18n', () => ({
    __esModule: true,
    default: {
        t: (key: string): string => key,
    },
}));

jest.mock('../insight/units/utils', () => ({
    getTimeOffset: jest.fn(() => 0),
}));

jest.mock('../insight/units/AscendUnit', () => ({
    ThreadUnit: class MockThreadUnit {
        metadata: unknown;
        isUnitVisible = true;
        isMerged = false;
        children = undefined;
        parent = undefined;

        constructor(metadata: unknown) {
            this.metadata = metadata;
        }
    },
}));

const DATA_SOURCE: DataSource = {
    remote: 'local',
    port: 0,
    projectName: 'project',
    dataPath: ['trace.db'],
    projectPath: [],
    children: [],
};

const createThreadUnit = (
    overrides: Partial<ThreadMetaData> = {},
    state: { isMerged?: boolean; isUnitVisible?: boolean } = {},
): InsightUnit => {
    const unit = new ThreadUnit({
        dataSource: DATA_SOURCE,
        cardId: '0',
        dbPath: 'trace.db',
        metaType: 'Ascend Hardware',
        processId: 'Ascend Hardware',
        processName: 'Ascend Hardware',
        threadId: '47',
        threadName: 'Stream 47',
        groupNameValue: '',
        rankList: [],
        ...overrides,
    }) as InsightUnit;
    unit.isMerged = state.isMerged ?? false;
    unit.isUnitVisible = state.isUnitVisible ?? true;
    return unit;
};

const createSelectedData = (overrides: Partial<SelectedDataType> = {}): SelectedDataType => ({
    id: '1',
    name: 'MODEL_EXECUTE',
    startTime: 100,
    duration: 10,
    depth: 0,
    threadId: '47',
    processId: 'Ascend Hardware',
    cardId: '0',
    dbPath: 'trace.db',
    metaType: 'Ascend Hardware',
    ...overrides,
});

const createSession = (
    sourceUnit: InsightUnit = createThreadUnit(),
    units: InsightUnit[] = [sourceUnit],
    selectedData: SelectedDataType = createSelectedData(),
): Session => ({
    selectedData,
    selectedUnits: [sourceUnit],
    selectedUnitKeys: [],
    units,
    domain: { timePerPx: 1, domainRange: { domainStart: 0, domainEnd: 1000 } },
    locateUnit: undefined,
} as unknown as Session);

const flushPromises = async (): Promise<void> => {
    for (let index = 0; index < 5; index++) {
        await Promise.resolve();
    }
};

const deferred = <T>(): { promise: Promise<T>; resolve: (value: T) => void } => {
    let resolvePromise: (value: T) => void = (): void => {};
    const promise = new Promise<T>((resolve) => {
        resolvePromise = resolve;
    });
    return { promise, resolve: resolvePromise };
};

describe('actionJumpToModelStream', () => {
    const requestMock = jest.fn();
    const warningMock = message.warning as jest.Mock;
    const getTimeOffsetMock = getTimeOffset as jest.Mock;
    const originalRequest = window.request;

    beforeEach(() => {
        requestMock.mockReset();
        warningMock.mockReset();
        getTimeOffsetMock.mockReturnValue(0);
        window.request = requestMock;
    });

    afterAll(() => {
        window.request = originalRequest;
    });

    it('is visible only for MODEL_EXECUTE tasks', () => {
        expect(actionJumpToModelStream.visible?.(createSession())).toBe(true);
        expect(actionJumpToModelStream.visible?.(
            createSession(undefined, undefined, createSelectedData({ name: 'model-execute' })))).toBe(true);
        expect(actionJumpToModelStream.visible?.(
            createSession(undefined, undefined, createSelectedData({ name: 'MODEL_WAIT_COMPLETE' })))).toBe(false);
        const lockedSession = createSession();
        lockedSession.selectedRangeIsLock = true;
        expect(actionJumpToModelStream.visible?.(lockedSession)).toBe(false);
    });

    it('does not request or locate while the selection is locked', async () => {
        const testSession = createSession();
        testSession.selectedRangeIsLock = true;

        actionJumpToModelStream.perform(testSession);
        await flushPromises();

        expect(requestMock).not.toHaveBeenCalled();
        expect(testSession.locateUnit).toBeUndefined();
    });

    it('locates a related stream and selects the source plus every same-model visible stream', async () => {
        requestMock.mockResolvedValue({
            data: { args: JSON.stringify({ modelId: '48' }), modelStreamIds: ['47', '12', '11'] },
        });
        const source = createThreadUnit();
        const stream11 = createThreadUnit({ threadId: '11', threadName: 'Stream 11' });
        const wrongCard = createThreadUnit({ cardId: '1', threadId: '11', threadName: 'Stream 11' });
        const stream12 = createThreadUnit({ threadId: '12', threadName: 'Stream 12' });
        const wrongProcess = createThreadUnit({ processId: 'Other', threadId: '12', threadName: 'Stream 12' });
        const unrelated = createThreadUnit({ threadId: '13', threadName: 'Stream 13' });
        const selectedData = createSelectedData();
        const testSession = createSession(
            source, [source, stream11, wrongCard, stream12, wrongProcess, unrelated], selectedData);
        const originalDomainRange = testSession.domain.domainRange;

        actionJumpToModelStream.perform(testSession);

        expect(requestMock).toHaveBeenCalledWith(DATA_SOURCE, {
            command: 'unit/threadDetail',
            params: expect.objectContaining({ tid: '47', id: '1', startTime: 100 }),
        });
        await flushPromises();

        expect(testSession.locateUnit?.target(stream11)).toBe(true);
        expect(testSession.locateUnit?.target(stream12)).toBe(false);
        testSession.locateUnit?.onSuccess(stream11);
        expect(testSession.selectedUnits).toEqual([source, stream11, stream12]);
        expect(testSession.selectedData).toBe(selectedData);
        expect(testSession.domain.domainRange).toBe(originalDomainRange);
        expect(testSession.locateUnit?.showDetail).toBe(false);
        expect(testSession.locateUnit?.tuneToSelectedSlice).toBe(false);
    });

    it('treats the MODEL_EXECUTE source stream as a valid single-stream target', async () => {
        requestMock.mockResolvedValue({
            data: { args: JSON.stringify({ modelId: '48' }), modelStreamIds: ['47'] },
        });
        const source = createThreadUnit();
        const testSession = createSession(source, [source]);

        actionJumpToModelStream.perform(testSession);
        await flushPromises();

        expect(testSession.locateUnit?.target(source)).toBe(true);
        testSession.locateUnit?.onSuccess(source);
        expect(testSession.selectedUnits).toEqual([source]);
        expect(warningMock).not.toHaveBeenCalled();
    });

    it('selects a visible legacy merged lane instead of its hidden source lanes', async () => {
        requestMock.mockResolvedValue({
            data: { args: JSON.stringify({ modelId: 48 }), modelStreamIds: ['11', '12'] },
        });
        const source = createThreadUnit();
        const hidden11 = createThreadUnit(
            { threadId: '11', threadName: 'Stream 11' }, { isMerged: true });
        const hidden12 = createThreadUnit(
            { threadId: '12', threadName: 'Stream 12' }, { isMerged: true });
        const merged = createThreadUnit({
            threadId: '',
            threadIdList: ['11', '12'],
            threadName: 'Stream Merged (11, 12)',
        });
        const testSession = createSession(source, [source, hidden11, merged, hidden12]);

        actionJumpToModelStream.perform(testSession);
        await flushPromises();

        expect(testSession.locateUnit?.target(merged)).toBe(true);
        testSession.locateUnit?.onSuccess(merged);
        expect(testSession.selectedUnits).toEqual([source, merged]);
    });

    it('warns when model information or its stream relation is unavailable', async () => {
        requestMock.mockResolvedValueOnce({
            data: { args: JSON.stringify({ modelId: '4294967295' }), modelStreamIds: ['47'] },
        });
        const invalidModelSession = createSession();
        actionJumpToModelStream.perform(invalidModelSession);
        await flushPromises();
        expect(warningMock).toHaveBeenCalledWith(
            'timeline:contextMenu.Model Execute Model ID Unavailable');

        requestMock.mockResolvedValueOnce({
            data: { args: JSON.stringify({ modelId: '48' }), modelStreamIds: [] },
        });
        const emptyRelationSession = createSession();
        actionJumpToModelStream.perform(emptyRelationSession);
        await flushPromises();
        expect(warningMock).toHaveBeenCalledWith(
            'timeline:contextMenu.Model Execute Stream Group Unavailable');

        requestMock.mockResolvedValueOnce({
            data: { args: JSON.stringify({ modelId: '48' }), modelStreamIds: ['99'] },
        });
        const unmatchedRelationSession = createSession();
        actionJumpToModelStream.perform(unmatchedRelationSession);
        await flushPromises();
        expect(warningMock).toHaveBeenLastCalledWith(
            'timeline:contextMenu.Model Execute Stream Group Unavailable');
        expect(unmatchedRelationSession.locateUnit).toBeUndefined();
    });

    it('relies on the global error center when the detail request fails', async () => {
        requestMock.mockRejectedValue(new Error('request failed'));
        const testSession = createSession();

        actionJumpToModelStream.perform(testSession);
        await flushPromises();

        expect(warningMock).not.toHaveBeenCalled();
        expect(testSession.locateUnit).toBeUndefined();
    });

    it('ignores a late detail response after another slice is selected', async () => {
        const detailResponse = deferred<{ data: { args: string; modelStreamIds: string[] } }>();
        requestMock.mockReturnValue(detailResponse.promise);
        const testSession = createSession();

        actionJumpToModelStream.perform(testSession);
        const newerSelectedData = createSelectedData({ id: '2', threadId: '12', startTime: 200 });
        const newerSelectedUnit = createThreadUnit({ threadId: '12', threadName: 'Stream 12' });
        testSession.selectedData = newerSelectedData;
        testSession.selectedUnits = [newerSelectedUnit];
        detailResponse.resolve({
            data: { args: JSON.stringify({ modelId: '48' }), modelStreamIds: ['11', '12', '47'] },
        });
        await flushPromises();

        expect(testSession.selectedData).toBe(newerSelectedData);
        expect(testSession.selectedUnits).toEqual([newerSelectedUnit]);
        expect(testSession.locateUnit).toBeUndefined();
        expect(warningMock).not.toHaveBeenCalled();
    });
});
