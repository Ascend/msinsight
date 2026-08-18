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

import type { ThreadMetaData } from '../../entity/data';
import type { InsightUnit } from '../../entity/insight';
import { findOperatorUnit, isOperatorMetadata } from '../operatorUnit';

const createThreadUnit = (metadata: Partial<ThreadMetaData>): InsightUnit => ({
    name: 'Thread',
    metadata,
} as unknown as InsightUnit);

describe('operator unit resolution', () => {
    it('resolves a record to a non-first selected child lane', () => {
        const firstUnit = createThreadUnit({
            cardId: '0', processId: '100', threadId: 'acl', metaType: 'CANN_API',
        });
        const targetUnit = createThreadUnit({
            cardId: '0', processId: 'Ascend Hardware', threadId: '2', metaType: 'Ascend Hardware',
        });

        const result = findOperatorUnit([firstUnit, targetUnit], {
            cardId: '0', pid: 'Ascend Hardware', tid: '2', metaType: undefined,
        });

        expect(result).toBe(targetUnit);
    });

    it('matches merged thread lists and Python Stack aliases', () => {
        const mergedMetadata = {
            cardId: '0', processId: 'Ascend Hardware', threadId: '', threadIdList: ['2', '3'], metaType: 'Ascend Hardware',
        } as ThreadMetaData;
        const pythonStackMetadata = {
            cardId: '0', processId: '100', threadId: 'python_stack:100', threadName: 'Python Stack 100', metaType: 'PYTORCH_API_PYTHON_STACK',
        } as ThreadMetaData;

        expect(isOperatorMetadata(mergedMetadata, {
            cardId: '0', pid: 'Ascend Hardware', tid: '3', metaType: 'Ascend Hardware',
        })).toBe(true);
        expect(isOperatorMetadata(pythonStackMetadata, {
            cardId: '0', pid: '100', tid: '100', metaType: undefined,
        })).toBe(true);
    });

    it('does not inherit the first lane meta type when the record has no meta type', () => {
        const targetMetadata = {
            cardId: '0', processId: '200', threadId: 'runtime', metaType: 'OSRT_API',
        } as ThreadMetaData;

        expect(isOperatorMetadata(targetMetadata, {
            cardId: '0', pid: '200', tid: 'runtime', metaType: undefined,
        })).toBe(true);
        expect(isOperatorMetadata(targetMetadata, {
            cardId: '0', pid: '200', tid: 'runtime', metaType: 'CANN_API',
        })).toBe(false);
    });
});
