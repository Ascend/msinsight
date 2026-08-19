/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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

import { runInAction } from 'mobx';
import i18n from '@insight/lib/i18n';
import connector from './index';
import {
    parseMemoryCompletedHandler,
    removeRemoteHandler,
    setTheme,
    updateSessionHandler,
    allSuccessHandler,
    deleteCardHandler,
    switchLanguageHandler,
    switchDirectoryHandler,
    getTimelineCardOffset,
} from './handler';
import type { MemoryRankInfo } from '../entity/memory';
import type { CardInfo, CardRankInfo } from '../entity/session';
import {
    customConsole as console,
} from '@insight/lib/utils';

// Import store after mocking
import { store } from '../store';

// Mock external dependencies
jest.mock('../store', () => ({
    store: {
        sessionStore: {
            activeSession: null,
        },
        memoryStore: {
            activeSession: null,
        },
    },
}));

jest.mock('@insight/lib/i18n');
jest.mock('@insight/lib/utils');
jest.mock('./index', () => ({
    __esModule: true,
    default: { send: jest.fn() },
}));

const mockConsoleError = jest.fn();

// Mock window.setTheme
Object.defineProperty(window, 'setTheme', {
    value: jest.fn(),
    writable: true,
});

// Create a helper to safely modify store properties
const setStoreSession = (session: any): void => {
    Object.defineProperty(store.sessionStore, 'activeSession', {
        value: session,
        writable: true,
        configurable: true,
    });
};

const setMemoryStoreSession = (session: any): void => {
    Object.defineProperty(store.memoryStore, 'activeSession', {
        value: session,
        writable: true,
        configurable: true,
    });
};

beforeEach(() => {
    jest.clearAllMocks();
    (console as any).error = mockConsoleError;

    // Reset store to initial state
    setStoreSession(null);
    setMemoryStoreSession(null);
});

describe('parseMemoryCompletedHandler', () => {
    const mockData = {
        memoryResult: [
            {
                rankInfo: { rankName: 'test', deviceId: 'test', rankId: '123' },
                dbPath: '/test/path',
                hasMemory: true,
            },
        ] as MemoryRankInfo[],
    };

    it('should update session memoryCardInfos when active session exists', async () => {
        const mockSession = {
            memoryCardInfos: [] as CardRankInfo[],
        };
        setStoreSession(mockSession);

        // 监控 runInAction
        const spy = jest.spyOn(require('mobx'), 'runInAction');
        await parseMemoryCompletedHandler(mockData);
        expect(spy).toHaveBeenCalled();
        // Verify that runInAction was called with a function
        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        expect(typeof actionFunction).toBe('function');

        // Execute the action function to verify its behavior
        actionFunction();
        expect(mockSession.memoryCardInfos).toBeDefined();
        spy.mockRestore();
    });

    it('should not throw error when no active session', async () => {
        setStoreSession(null);

        await expect(parseMemoryCompletedHandler(mockData)).resolves.not.toThrow();
    });

    it('should catch and log errors', async () => {
        const mockSession = {};
        setStoreSession(mockSession);
        const spy = jest.spyOn(require('mobx'), 'runInAction');
        (spy as jest.Mock).mockImplementation((fn) => {
            throw new Error('Test error');
        });

        await parseMemoryCompletedHandler(mockData);

        expect(mockConsoleError).toHaveBeenCalled();
        spy.mockRestore();
    });
});

describe('removeRemoteHandler', () => {
    it('should reset session properties when active sessions exist', async () => {
        const mockSession = {
            memoryCardInfos: ['item1', 'item2'],
            isAllMemoryCompletedSwitch: true,
            isCluster: true,
            compareRank: { rankId: '123', isCompare: true },
        };

        const mockMemorySession = {
            rankCondition: { options: [1, 2, 3], value: 1 },
        };

        setStoreSession(mockSession);
        setMemoryStoreSession(mockMemorySession);
        const spy = jest.spyOn(require('mobx'), 'runInAction');
        await removeRemoteHandler({});
        const actionFunction = (spy as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.memoryCardInfos).toEqual([]);
        expect(mockSession.isAllMemoryCompletedSwitch).toBe(false);
        expect(mockSession.isCluster).toBe(false);
        expect(mockSession.compareRank.rankId).toBe('');
        expect(mockMemorySession.rankCondition).toEqual({ options: [], value: 0 });
        spy.mockRestore();
    });

    it('should do nothing when no active session', async () => {
        setStoreSession(null);
        setMemoryStoreSession(null);
        const spy = jest.spyOn(require('mobx'), 'runInAction');
        await removeRemoteHandler({});

        const actionFunction = (spy as jest.Mock).mock.calls[0][0];
        actionFunction();

        // No changes should be made
        expect(store.sessionStore.activeSession).toBeNull();
        spy.mockRestore();
    });
});

describe('getTimelineCardOffset', () => {
    it('requests the selected card Device offset without exposing an internal key', () => {
        setMemoryStoreSession({
            rankCondition: {
                value: 0,
                options: [{ rankInfo: { rankId: 'host0 7' } }],
            },
            selectedRankId: 'host0 7',
        });

        getTimelineCardOffset();

        expect(connector.send).toHaveBeenCalledWith({
            event: 'getTimelineCardOffset',
            to: 'Timeline',
            body: {
                cardId: 'host0 7',
                side: 'device',
            },
        });
    });
});

describe('setTheme', () => {
    it('should call window.setTheme with correct parameter', () => {
        const mockData = { isDark: true };

        setTheme(mockData);

        expect(window.setTheme).toHaveBeenCalledWith(true);
    });

    it('should handle false value correctly', () => {
        const mockData = { isDark: false };

        setTheme(mockData);

        expect(window.setTheme).toHaveBeenCalledWith(false);
    });
});

describe('updateSessionHandler', () => {
    const mockSession = {
        memoryCardInfos: [] as CardRankInfo[],
        isCluster: false,
        unitcount: 0,
    };

    beforeEach(() => {
        mockSession.memoryCardInfos = [];
        mockSession.isCluster = false;
        mockSession.unitcount = 0;
        setStoreSession(mockSession);
    });

    it('should update memoryCardInfos with sorted array', async () => {
        const mockData = {
            memoryCardInfos: [
                { rankInfo: { rankId: '1' }, index: 2, dbPath: '/path1' },
                { rankInfo: { rankId: '2' }, index: 1, dbPath: '/path2' },
            ] as CardRankInfo[],
        };

        jest.spyOn(require('mobx'), 'runInAction');
        await updateSessionHandler(mockData);

        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.memoryCardInfos).toEqual([
            { rankInfo: { rankId: '2' }, index: 1, dbPath: '/path2' },
            { rankInfo: { rankId: '1' }, index: 2, dbPath: '/path1' },
        ]);
    });

    it('should update usable keys from data', async () => {
        const mockData = {
            isCluster: true,
            unitcount: 5,
            someOtherKey: 'should be ignored',
        };

        jest.spyOn(require('mobx'), 'runInAction');
        await updateSessionHandler(mockData);

        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.isCluster).toBe(true);
        expect(mockSession.unitcount).toBe(5);
        expect((mockSession as any).someOtherKey).toBeUndefined();
    });

    it('should handle empty data object', async () => {
        jest.spyOn(require('mobx'), 'runInAction');
        const spy = jest.spyOn(require('mobx'), 'runInAction');
        await updateSessionHandler({});

        const actionFunction = (spy as jest.Mock).mock.calls[0][0];
        actionFunction();

        // No changes should occur
        expect(mockSession.memoryCardInfos).toEqual([]);
    });
});

describe('allSuccessHandler', () => {
    it('should toggle isAllMemoryCompletedSwitch when isAllPageParsed is true', async () => {
        const mockSession = {
            isAllMemoryCompletedSwitch: false,
        };
        setStoreSession(mockSession);

        await allSuccessHandler({ isAllPageParsed: true });

        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.isAllMemoryCompletedSwitch).toBe(true);
    });

    it('should not toggle when isAllPageParsed is false', async () => {
        const mockSession = {
            isAllMemoryCompletedSwitch: false,
        };
        setStoreSession(mockSession);

        await allSuccessHandler({ isAllPageParsed: false });

        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.isAllMemoryCompletedSwitch).toBe(false);
    });

    it('should handle missing isAllPageParsed property', async () => {
        const mockSession = {
            isAllMemoryCompletedSwitch: false,
        };
        setStoreSession(mockSession);

        await allSuccessHandler({});

        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.isAllMemoryCompletedSwitch).toBe(false);
    });
});

describe('deleteCardHandler', () => {
    it('should remove specified cards from memoryCardInfos', async () => {
        const mockSession = {
            memoryCardInfos: [
                { rankInfo: { rankId: '1' } },
                { rankInfo: { rankId: '2' } },
                { rankInfo: { rankId: '3' } },
            ] as CardRankInfo[],
        };
        setStoreSession(mockSession);

        const mockData = {
            info: [
                { cardId: '1' },
                { cardId: '3' },
            ] as CardInfo[],
        };

        await deleteCardHandler(mockData);

        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.memoryCardInfos).toHaveLength(1);
        expect(mockSession.memoryCardInfos[0].rankInfo.rankId).toBe('2');
    });

    it('should handle empty delete list', async () => {
        const mockSession = {
            memoryCardInfos: [
                { rankInfo: { rankId: '1' } },
            ] as CardRankInfo[],
        };
        setStoreSession(mockSession);

        await deleteCardHandler({ info: [] });

        const actionFunction = (runInAction as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.memoryCardInfos).toHaveLength(1);
    });
});

describe('switchLanguageHandler', () => {
    it('should update session language and change i18n language', () => {
        const mockSession = {
            language: 'zhCN',
        };
        const spy = jest.spyOn(require('mobx'), 'runInAction');
        setStoreSession(mockSession);

        switchLanguageHandler({ lang: 'enUS' });

        const actionFunction = (spy as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.language).toBe('enUS');
        expect(i18n.changeLanguage).toHaveBeenCalledWith('enUS');
    });

    it('should only change i18n language when no active session', () => {
        const spy = jest.spyOn(require('mobx'), 'runInAction');
        setStoreSession(null);
        switchLanguageHandler({ lang: 'zhCN' });

        expect(i18n.changeLanguage).toHaveBeenCalledWith('zhCN');
        // runInAction should still be called but the function inside won't modify session
        expect(spy).toHaveBeenCalledTimes(0);
    });
});

describe('switchDirectoryHandler', () => {
    it('should update compareRank when active session exists', () => {
        const mockSession = {
            compareRank: { rankId: '', isCompare: false },
        };
        setStoreSession(mockSession);
        const spy = jest.spyOn(require('mobx'), 'runInAction');
        switchDirectoryHandler({ rankId: 'newRank', isCompare: true });

        const actionFunction = (spy as jest.Mock).mock.calls[0][0];
        actionFunction();

        expect(mockSession.compareRank).toEqual({
            rankId: 'newRank',
            isCompare: true,
        });
    });

    it('should do nothing when no active session', () => {
        setStoreSession(null);
        jest.spyOn(require('mobx'), 'runInAction');
        switchDirectoryHandler({ rankId: 'newRank', isCompare: true });

        // runInAction should still be called but the function inside won't modify session
        expect(runInAction).toHaveBeenCalledTimes(0);
    });
});

// Error handling tests
describe('Error Handling', () => {
    beforeEach(() => {
        setStoreSession({});
    });

    it('should catch errors in parseMemoryCompletedHandler', async () => {
        (runInAction as jest.Mock).mockImplementation(() => {
            throw new Error('Test error');
        });

        await expect(parseMemoryCompletedHandler({})).resolves.not.toThrow();
        expect(mockConsoleError).toHaveBeenCalled();
    });

    it('should catch errors in removeRemoteHandler', async () => {
        (runInAction as jest.Mock).mockImplementation(() => {
            throw new Error('Test error');
        });

        await expect(removeRemoteHandler({})).resolves.not.toThrow();
        expect(mockConsoleError).toHaveBeenCalled();
    });

    it('should catch errors in updateSessionHandler', async () => {
        (runInAction as jest.Mock).mockImplementation(() => {
            throw new Error('Test error');
        });

        await expect(updateSessionHandler({})).resolves.not.toThrow();
        expect(mockConsoleError).toHaveBeenCalled();
    });
});
