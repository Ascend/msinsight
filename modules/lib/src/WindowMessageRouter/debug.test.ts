/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    clearWindowMessageDebugRecords,
    getWindowMessageDebugRecords,
    recordWindowMessageDebug,
    setWindowMessageDebugEnabled,
    subscribeWindowMessageDebug,
} from './index';

describe('WindowMessageDebug', () => {
    test('records bounded communication metadata and supports clearing', () => {
        clearWindowMessageDebugRecords();
        setWindowMessageDebugEnabled(true);
        const listener = jest.fn();
        const unsubscribe = subscribeWindowMessageDebug(listener);
        try {
            recordWindowMessageDebug({
                direction: 'outbound',
                data: {
                    channel: 'moduleAgentMessage',
                    event: 'moduleAgent/executeCommand',
                    moduleId: 'MemScope',
                    requestId: 'request-1',
                    command: 'MemScope.table.refresh',
                    connectionToken: 'token-1',
                },
                target: 'MemScope',
            });
            expect(getWindowMessageDebugRecords()).toEqual([
                expect.objectContaining({
                    direction: 'outbound',
                    channel: 'moduleAgentMessage',
                    event: 'moduleAgent/executeCommand',
                    moduleId: 'MemScope',
                    requestId: 'request-1',
                    command: 'MemScope.table.refresh',
                    connectionToken: 'token-1',
                    target: 'MemScope',
                }),
            ]);
            clearWindowMessageDebugRecords();
            expect(getWindowMessageDebugRecords()).toEqual([]);
            expect(listener).toHaveBeenLastCalledWith([]);
        } finally {
            unsubscribe();
            setWindowMessageDebugEnabled(false);
            clearWindowMessageDebugRecords();
        }
    });
});
