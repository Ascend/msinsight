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

import connector from '../connection';
import { actionRemoveTimeRangeAnalysis, actionTimeRangeAnalysis } from './actionTimeRangeAnalysis';

jest.mock('../connection', () => ({
    __esModule: true,
    default: { send: jest.fn() },
}));

const createSession = (): any => ({
    selectedRange: [100, 200],
    selectedUnits: [{}],
    isTimeAnalysisMode: false,
    mKeyRender: false,
    mMaskRange: [],
    timeAnalysisRange: [100, 200],
});

beforeEach(() => jest.clearAllMocks());

test.each([
    ['selects', actionTimeRangeAnalysis, [100, 200]],
    ['clears', actionRemoveTimeRangeAnalysis, null],
])('%s the Timeline range for other modules', (_name, action, expectedRange) => {
    const session = createSession();
    action.perform(session);

    expect(connector.send).toHaveBeenCalledWith({
        event: 'updateSession',
        body: { timeAnalysisRange: expectedRange },
    });
});
