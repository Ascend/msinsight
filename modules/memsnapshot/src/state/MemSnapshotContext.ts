/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import type { MemoryContext } from '../common/shared/state/MemoryContext';
import type { MemSnapshotState } from './MemSnapshotState';
// Structural view of the compatibility session: common state plus this feature's fields only.
export interface MemSnapshotContext extends MemoryContext, MemSnapshotState {
    stateWorkerInfo: {
        renderOptions: RenderOptions;
        hoverItem: StateDataHoverResult | null;
        clickItem: StateDataHoverResult | null;
        eventId: number;
    };
}
