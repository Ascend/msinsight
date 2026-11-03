/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { mountMemoryApplication } from './common/application/mountMemoryApplication';
import MemScopePage from './pages/MemScopePage';
import { createAdapter } from './application/adapter';

declare const module: { hot?: { dispose: (callback: () => void) => void } };
const dispose = mountMemoryApplication(createAdapter(), MemScopePage);
module.hot?.dispose(dispose);
