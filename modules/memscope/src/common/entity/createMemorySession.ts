/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { makeAutoObservable } from 'mobx';
import { MemorySession } from './MemorySession';

// Feature fields delegate to their own observable state, retaining ref annotations and subscriptions.
export const createMemorySession = <T extends object>(feature: T, module: MemorySession['module']): MemorySession & T => {
    const session = new MemorySession() as MemorySession & T;
    session.module = module;
    // Observe the fixed common shape before adding feature accessors (MobX caches prototype keys).
    makeAutoObservable(session);
    const featureKeys = Object.keys(feature) as Array<keyof T>;

    featureKeys.forEach(key => {
        Object.defineProperty(session, key, {
            enumerable: true,
            configurable: true,
            get: () => feature[key],
            set: (value: T[typeof key]) => { feature[key] = value; },
        });
    });

    return session;
};
