/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { createMemorySession } from '../common/entity/createMemorySession';
import { MemSnapshotState } from '../state/MemSnapshotState';

export const createSession = (): ReturnType<typeof createMemorySession<MemSnapshotState>> =>
    createMemorySession(new MemSnapshotState(), 'memsnapshot');
export type MemSnapshotSession = ReturnType<typeof createSession>;
