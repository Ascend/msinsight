/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export const withAbortSignal = <T,>(
    execution: PromiseLike<T> | T,
    signal: AbortSignal,
    fallbackError: () => unknown,
): Promise<T> => {
    if (signal.aborted) return Promise.reject(signal.reason ?? fallbackError());
    return new Promise<T>((resolve, reject) => {
        const abort = (): void => reject(signal.reason ?? fallbackError());
        signal.addEventListener('abort', abort, { once: true });
        Promise.resolve(execution).then(resolve, reject).finally(() => signal.removeEventListener('abort', abort));
    });
};
