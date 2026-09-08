/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

export function deferred<T>(): { promise: Promise<T>; resolve: (value: T) => void; reject: (reason: unknown) => void } {
    const settlers: {
        fulfill?: (value: T) => void;
        fail?: (reason: unknown) => void;
    } = {};
    const promise = new Promise<T>((fulfill, fail) => {
        settlers.fulfill = fulfill;
        settlers.fail = fail;
    });
    return {
        promise,
        resolve: (value: T): void => settlers.fulfill?.(value),
        reject: (reason: unknown): void => settlers.fail?.(reason),
    };
}
