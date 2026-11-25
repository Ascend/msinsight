/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
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
