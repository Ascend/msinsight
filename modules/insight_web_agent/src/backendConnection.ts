/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import type { AcpUnavailableReason } from './acpStatus';

export interface BackendConnectionFailure {
    url: string;
    cause?: string;
    status?: AcpUnavailableReason;
    nodeVersion?: string;
}

type BackendConnectionListener = (failure: BackendConnectionFailure | undefined) => void;

const listeners = new Set<BackendConnectionListener>();
let lastFailure: BackendConnectionFailure | undefined;

export const reportBackendUnavailable = (failure: BackendConnectionFailure): void => {
    if (lastFailure) {
        return;
    }
    lastFailure = failure;
    listeners.forEach((listener) => listener(failure));
};

export const reportBackendAvailable = (): void => {
    if (!lastFailure) {
        return;
    }
    lastFailure = undefined;
    listeners.forEach((listener) => listener(undefined));
};

export const subscribeBackendUnavailable = (listener: BackendConnectionListener): (() => void) => {
    listeners.add(listener);
    listener(lastFailure);
    return () => listeners.delete(listener);
};
