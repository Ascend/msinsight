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

import { shouldQueryLeaksParseStatus } from './leaksParseDeleteGuard';

jest.mock('@/connection', () => ({
    __esModule: true,
    default: {
        send: jest.fn(),
        addListener: jest.fn(() => ({ event: 'leaksParseStatus', sequence: 0 })),
        removeListener: jest.fn(),
    },
}), { virtual: true });

jest.mock('@/moduleConfig', () => ({
    MEM_SCOPE_MODULE_NAME: 'MemScope',
}), { virtual: true });

jest.mock('@insight/lib/i18n', () => ({
    __esModule: true,
    default: { t: (key: string) => key },
}), { virtual: true });

jest.mock('antd', () => ({
    message: { warning: jest.fn() },
}));

describe('shouldQueryLeaksParseStatus', () => {
    const session = {
        isLeaks: true,
        activeDataSource: { projectName: 'current' },
    };

    it('only queries leaks when deleting the active project', () => {
        expect(shouldQueryLeaksParseStatus(session, { projectName: 'current' })).toBe(true);
        expect(shouldQueryLeaksParseStatus(session, { projectName: 'other' })).toBe(false);
        expect(shouldQueryLeaksParseStatus({ ...session, isLeaks: false }, { projectName: 'current' })).toBe(false);
        expect(shouldQueryLeaksParseStatus(session, { projectNames: ['other'] })).toBe(false);
        expect(shouldQueryLeaksParseStatus(session, { projectNames: ['current'] })).toBe(true);
        expect(shouldQueryLeaksParseStatus(session, { projectNames: [] })).toBe(true);
    });
});
