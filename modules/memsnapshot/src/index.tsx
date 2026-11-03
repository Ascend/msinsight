/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { mountMemoryApplication } from './common/application/mountMemoryApplication';
import MemSnapshotPage from './pages/MemSnapshotPage';
import { createAdapter } from './application/adapter';

declare const module: { hot?: { dispose: (callback: () => void) => void } };
const dispose = mountMemoryApplication(createAdapter(), MemSnapshotPage);
module.hot?.dispose(dispose);
