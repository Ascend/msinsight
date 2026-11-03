/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { createMemorySession } from '../common/entity/createMemorySession';
import { MemScopeState } from '../state/MemScopeState';

export const createSession = (): ReturnType<typeof createMemorySession<MemScopeState>> =>
    createMemorySession(new MemScopeState(), 'leaks');
export type MemScopeSession = ReturnType<typeof createSession>;
