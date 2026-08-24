/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export interface BackendConnectionFailure {
    url: string;
    cause?: string;
}

type BackendConnectionListener = (failure: BackendConnectionFailure) => void;

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
    lastFailure = undefined;
};

export const subscribeBackendUnavailable = (listener: BackendConnectionListener): (() => void) => {
    listeners.add(listener);
    if (lastFailure) {
        listener(lastFailure);
    }
    return () => listeners.delete(listener);
};
