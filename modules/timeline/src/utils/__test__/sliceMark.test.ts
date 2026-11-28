/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 * -------------------------------------------------------------------------
 */

import { message } from 'antd';
import type { Session } from '../../entity/session';
import { getTimeOffset } from '../../insight/units/utils';
import jumpToUnitOperator from '../jumpToUnitOperator';
import {
    createSliceMark,
    handleSliceMarkKey,
    handleSliceMarkShortcut,
    isSliceMarkKey,
    isEditableTarget,
    removeSliceMarksForUnits,
} from '../sliceMark';

jest.mock('antd', () => ({
    message: { warning: jest.fn() },
}));

jest.mock('@insight/lib/i18n', () => ({
    __esModule: true,
    default: { t: (_key: string, options: { key: string }): string => 'already-bound:' + options.key },
}));

jest.mock('../../insight/units/utils', () => ({
    getTimeOffset: jest.fn(() => 5),
}));

jest.mock('../jumpToUnitOperator', () => ({
    __esModule: true,
    default: jest.fn(),
}));

const createSession = (): Session => ({
    selectedData: {
        id: 'slice-1',
        name: 'matmul',
        startTime: 95,
        timestamp: 100,
        rawStartTime: '0',
        duration: 20,
        depth: 2,
        threadId: '7',
        processId: '11',
        metaType: 'Ascend Hardware',
    },
    selectedDataUnit: {
        metadata: {
            cardId: 'card-0',
            dbPath: 'trace.db',
            threadId: '7',
            processId: '11',
            metaType: 'Ascend Hardware',
        },
    },
    sliceMarks: new Map(),
} as unknown as Session);

describe('slice mark shortcuts', () => {
    beforeEach(() => {
        jest.clearAllMocks();
        (getTimeOffset as jest.Mock).mockReturnValue(5);
    });

    it('keeps the original slice time when the detail timestamp is invalid', () => {
        const session = createSession();
        const mark = createSliceMark(session, '1');

        expect((mark?.operator.timestamp ?? Number.NaN) - 5).toBe(95);
        expect(mark).toEqual({
            key: '1',
            name: 'matmul',
            operator: {
                id: 'slice-1',
                cardId: 'card-0',
                dbPath: 'trace.db',
                tid: '7',
                pid: '11',
                depth: 2,
                duration: 20,
                name: 'matmul',
                timestamp: 100,
                metaType: 'Ascend Hardware',
            },
        });
        expect(session.sliceMarks.get('1')).toEqual(mark);
    });

    it('jumps to an existing mark instead of replacing it', () => {
        const session = createSession();
        const first = handleSliceMarkKey(session, '1');
        const second = handleSliceMarkKey(session, '1');

        expect(first).toBe('created');
        expect(second).toBe('jumped');
        expect(session.sliceMarks.size).toBe(1);
        expect(jumpToUnitOperator).toHaveBeenCalledWith(session.sliceMarks.get('1')?.operator);
    });

    it('ignores invalid keys and selections', () => {
        const session = createSession();
        session.selectedData = undefined;

        expect(isSliceMarkKey('0')).toBe(false);
        expect(handleSliceMarkKey(session, '0')).toBe('ignored');
        expect(handleSliceMarkKey(session, '1')).toBe('ignored');
        expect(session.sliceMarks.size).toBe(0);
    });

    it('does not treat editable elements as shortcut targets', () => {
        const input = document.createElement('input');
        const div = document.createElement('div');
        div.contentEditable = 'true';

        expect(isEditableTarget(input)).toBe(true);
        expect(isEditableTarget(div)).toBe(true);
        expect(isEditableTarget(document.createElement('canvas'))).toBe(false);
    });

    it('handles a digit key once and ignores repeated or modified events', () => {
        const session = createSession();
        const event = new KeyboardEvent('keydown', { key: '1', cancelable: true });
        expect(handleSliceMarkShortcut(session, event)).toBe(true);
        expect(event.defaultPrevented).toBe(true);
        expect(handleSliceMarkShortcut(session, new KeyboardEvent('keydown', { key: '2', repeat: true }))).toBe(false);
        expect(handleSliceMarkShortcut(session, new KeyboardEvent('keydown', { key: '3', ctrlKey: true }))).toBe(false);
    });
    it('does not bind another shortcut to the same slice and reports the existing key', () => {
        const session = createSession();

        expect(handleSliceMarkShortcut(session, new KeyboardEvent('keydown', { key: '1', cancelable: true }))).toBe(true);
        expect(handleSliceMarkShortcut(session, new KeyboardEvent('keydown', { key: '2', cancelable: true }))).toBe(true);

        expect(Array.from(session.sliceMarks.keys())).toEqual(['1']);
        expect(message.warning).toHaveBeenCalledWith('already-bound:1');
    });

    it('keeps de-duplication stable when the offset changes after a jump', () => {
        const session = createSession();
        // After jumpToUnitOperator the selection keeps the absolute timestamp (100) but the
        // display startTime and the current offset can drift (e.g. offset restored to 0).
        (getTimeOffset as jest.Mock).mockReturnValue(0);
        const selected = session.selectedData as NonNullable<Session['selectedData']>;
        selected.startTime = 90;
        selected.timestamp = 100;

        expect(handleSliceMarkShortcut(session, new KeyboardEvent('keydown', { key: '1', cancelable: true }))).toBe(true);
        expect(handleSliceMarkShortcut(session, new KeyboardEvent('keydown', { key: '2', cancelable: true }))).toBe(true);

        expect(Array.from(session.sliceMarks.keys())).toEqual(['1']);
        expect(session.sliceMarks.get('1')?.operator.timestamp).toBe(100);
        expect(message.warning).toHaveBeenCalledWith('already-bound:1');
    });

    it('drops marks whose source card was removed and keeps the others', () => {
        const session = createSession();
        const markOf = (key: '1' | '2', cardId: string): void => {
            session.sliceMarks.set(key, {
                key,
                name: `op-${cardId}`,
                operator: {
                    id: `slice-${cardId}`,
                    cardId,
                    dbPath: 'trace.db',
                    tid: '7',
                    pid: '11',
                    depth: 1,
                    duration: 5,
                    name: `op-${cardId}`,
                    timestamp: 100,
                    metaType: 'Ascend Hardware',
                },
            });
        };
        markOf('1', 'card-0');
        markOf('2', 'card-1');

        removeSliceMarksForUnits(session, [{ cardId: 'card-0', dbPath: 'trace.db' }]);

        expect(Array.from(session.sliceMarks.keys())).toEqual(['2']);
    });
});
