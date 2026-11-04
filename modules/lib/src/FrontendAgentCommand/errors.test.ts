/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { toCommandError } from './errors';

test('plain errors have a useful message even without a full command shape', () => {
    expect(toCommandError({ message: 'backend query failed' })).toEqual({
        code: 'COMMAND_EXECUTION_FAILED', message: 'backend query failed', retryable: false,
    });
    expect(toCommandError({}).message).not.toBe('[object Object]');
    let reads = 0;
    expect(toCommandError({ get message() { reads++; throw new Error('getter'); } }).message).toBe('Command execution failed.');
    expect(reads).toBe(0);
});
