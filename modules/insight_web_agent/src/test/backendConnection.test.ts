/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import {
    clearBackendConnectionFailure,
    reportBackendAvailable,
    reportBackendUnavailable,
    subscribeBackendUnavailable,
} from '../backendConnection';

afterEach(() => {
    clearBackendConnectionFailure();
});

test('reportBackendAvailable notifies subscribers so the dialog can close', () => {
    const listener = jest.fn();
    const unsubscribe = subscribeBackendUnavailable(listener);

    expect(listener).toHaveBeenCalledWith(undefined);
    reportBackendUnavailable({ url: 'http://127.0.0.1:9090/api/state', status: 'unreachable' });
    expect(listener).toHaveBeenCalledWith(expect.objectContaining({ status: 'unreachable' }));

    reportBackendAvailable();
    expect(listener).toHaveBeenLastCalledWith(undefined);

    unsubscribe();
});

test('does not auto-clear a missing or unsupported Node.js failure', () => {
    const listener = jest.fn();
    const unsubscribe = subscribeBackendUnavailable(listener);

    reportBackendUnavailable({ url: 'acp', status: 'missing-node' });
    reportBackendAvailable();
    expect(listener).toHaveBeenLastCalledWith(expect.objectContaining({ status: 'missing-node' }));

    clearBackendConnectionFailure();
    reportBackendUnavailable({ url: 'acp', status: 'unsupported-node', nodeVersion: '18.20.0' });
    reportBackendAvailable();
    expect(listener).toHaveBeenLastCalledWith(expect.objectContaining({ status: 'unsupported-node' }));

    unsubscribe();
});
